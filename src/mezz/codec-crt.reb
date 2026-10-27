REBOL [
	title:   "Codec: CRT"
	name:    crt
	type:    module
	version: 0.0.1
	Purpose: "Codec for X.509 Public Key Infrastructure Certificate"
	author:  "Oldes"
]

register-codec [
	name:  'crt
	type:  'cryptography
	title: "Internet X.509 Public Key Infrastructure Certificate and Certificate Revocation List (CRL) Profile"
	suffixes: [%.crt]

	get-fingerprint: function[
		"Hash of the tbsCertificate (the part covered by the signature)"
		data [binary!] method [word!]
	][
		bin: binary data
		loop 2 [
			binary/read bin [
				pos:      INDEX
				flags:    UI8
				length:   LENGTH
				start:    INDEX
			]
			if any [
				flags <> 48 ; 0x30 = class universal, constructed, SEQUENCE
				length > length? bin/buffer
			][	return none ]
			any [first-pos first-pos: pos]
		]
		;; pos, start and length now describe the tbsCertificate SEQUENCE
		;; (header size varies with the length encoding)
		checksum/part at data (pos - first-pos + 1) :method (start - pos + length)
	]
	decode: wrap [
		*oid:
		*val:
		*bin: none
		*bool: false
		*new: none
		*blk: copy []

		der-codec: system/codecs/DER
		
		Name: [
			(clear *blk)
			'SEQUENCE into [
				any [ ;; subject may be empty (RFC 5280 4.1.2.6)
					'SET into [
						some [ ;; multi-valued RDN
							'SEQUENCE into [
								'OBJECT_IDENTIFIER set *oid binary!
								word! set *val skip ;; any DirectoryString (unknown types stay binary)
								(
									*oid: der-codec/decode-OID *oid
									if word? *oid [*oid: to set-word! *oid]
									;?? *oid
									repend *blk [*oid *val]
								)
							]
						]
					]
				]
			]
			(new-line/skip *blk true 2)
		]

		AlgorithmIdentifier: [
			(clear *blk)
			'SEQUENCE into [
				'OBJECT_IDENTIFIER set *oid binary!
				[	;- optional parameters
					'NULL binary! (*val: none)
					|
					copy *val to end ;@@ TODO: decode it too?
				]
				(
					append *blk der-codec/decode-OID *oid
					if *val [append *blk *val]
				)
			]
			(new-line/skip *blk true 2)
		]

		Extensions: [
			(clear *blk)
			'SEQUENCE into [
				some [
					(*bool: false)
					'SEQUENCE into [
						'OBJECT_IDENTIFIER set *oid binary!
						opt ['BOOLEAN #{FF} (*bool: true)]
						'OCTET_STRING set *val binary! (
							*oid: der-codec/decode-OID *oid
							if all [not empty? *val *val/1 = 48] [*val: der-codec/decode *val]
							switch *oid [
								extendedKeyUsage [
									*new: copy []
									parse *val [
										'SEQUENCE into [
											some ['OBJECT_IDENTIFIER set *bin binary! (
												append *new der-codec/decode-OID *bin
											)]
										]
									]
									*val: *new
								]
								extKeyUsage [
									*val: der-codec/decode-OID *val/SEQUENCE/OBJECT_IDENTIFIER
								]
								subjectAltName [
									*new: copy []
									parse *val [
										'SEQUENCE into [
											any [
											  'CS2 set *bin binary! (append *new to string! *bin)            ;; dNSName
											| 'CS7 set *bin binary! (                                        ;; iPAddress
												append *new either 4 = length? *bin [to tuple! *bin][*bin] ;; IPv6 stays binary
											)
											| 'CS1 set *bin binary! (append *new to email! to string! *bin) ;; rfc822Name
											| 'CS6 set *bin binary! (append *new to url! to string! *bin)   ;; URI
											| skip skip ;; otherName, directoryName,... not decoded yet
										]
										]
									]
									*val: new-line/all *new true
								]
								;authorityInfoAccess [
								;	probe *val
								;]

							]
							repend *blk [*oid reduce [*bool *val]]
							new-line last *blk true
						)
					]
				]
			]
			(new-line/skip *blk true 2)
		]

		func [
			data [binary! block!]
			/local pkix der result tmp hash
		][
			try [all [
				; as there seems to be no standard, the *.crt file
				; may be actually in pkix format, so try it first...
				pkix: codecs/pkix/decode data
				pkix/label = "CERTIFICATE"
				data: pkix/binary
			]]

			der: either binary? data [
				der-codec/decode data
			][	data ]
			unless block? der [return none] ;; not DER data
			if all [
				2 = length? der
				'SEQUENCE = der/1 
				block? der/2 
			] [der: der/2]

			result: construct [
				version:
				serial-number:
				fingerprint:
				algorithm:
				issuer:
				valid-from:
				valid-to:
				subject:
				public-key:
				issuer-id:
				subject-id:
				extensions:
				signature:
			]

			unless parse der [
				'SEQUENCE into [
					;-- version:
					[
						'CS0 into [
							'INTEGER set *val binary! (
								result/version: to integer! *val
							)
						]
						| (result/version: 0) ;; DEFAULT v1 (absent in v1 certificates)
					]

					;-- serial number:
					'INTEGER set *val binary! (	result/serial-number: *val	)
					
					;-- signature:
					AlgorithmIdentifier ( result/algorithm: copy *blk )

					;-- issuer:
					Name (result/issuer: copy *blk)
					
					;-- validity:
					'SEQUENCE into [
						['UTC_TIME | 'GENERALIZED_TIME] set *val date! (result/valid-from: *val)
						['UTC_TIME | 'GENERALIZED_TIME] set *val date! (result/valid-to:   *val)
					]
					;-- subject:
					Name (result/subject: copy *blk)

					;-- subject public key info:
					'SEQUENCE into [
						AlgorithmIdentifier ( result/public-key: copy *blk )
						'BIT_STRING set *val binary! (
							append/only result/public-key switch/default *blk/1 [
								rsaEncryption [
									tmp: der-codec/decode *val
									reduce [copy tmp/2/2 copy tmp/2/4] ;@@ or don't copy?
								]
								ecPublicKey [
									remove next result/public-key
									result/public-key/2: der-codec/decode-OID result/public-key/2
									copy *val
								]
							][	copy *val ]
							
						)
					]

					;; [1] and [2] IMPLICIT BIT STRING
					opt ['CS1 set *val binary! ( result/issuer-id: *val )]
					opt ['CS2 set *val binary! ( result/subject-id: *val )]
					opt [
						;-- extensions
						'CS3 into [
							Extensions ( result/extensions: copy *blk )
						]
					]
					to end
				]

				;-- signature:
				AlgorithmIdentifier (
					;; MUST be the same as the tbsCertificate signature algorithm (RFC 5280 4.1.1.2)
					if *blk <> result/algorithm [return none]
					if 1 = length? result/algorithm [
						result/algorithm: first result/algorithm
					]
				)
				'BIT_STRING set *val binary! ( result/signature: *val )
				to end
			][	return none ] ;; not a (supported) X.509 certificate
			if all [
				binary? data
				hash: select [
					sha256WithRSAEncryption sha256
					sha384WithRSAEncryption sha384
					sha512WithRSAEncryption sha512
					md5withRSAEncryption    md5
					md4withRSAEncryption    md4
					ecdsa-with-SHA224       sha224
					ecdsa-with-SHA256       sha256
					ecdsa-with-SHA384       sha384
					ecdsa-with-SHA512       sha512
					sha224WithRSAEncryption sha224
					sha1WithRSAEncryption   sha1
					ecdsa-with-SHA1         sha1
				] result/algorithm
			][
				try [
					result/fingerprint: get-fingerprint :data :hash
				]
			]
			if verbose > 0 [
				prin "^/^[[1;32mCRT"
				either verbose > 1 [
					?? result
				][
					print " result:^[[0m"
					print dump-obj result
				]
			]
			result
		]
	]
	verbose: 0
]