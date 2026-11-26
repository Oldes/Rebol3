# SAFE: Encrypted Persistent Storage

SAFE is Rebol's built-in way to keep small amounts of private data
(passwords, API tokens, settings) in an encrypted file. It has three
layers:

* the **`safe` codec**: the encrypted file format (`.safe` files)
* the **`safe` scheme**: a persistent key/value store on top of the codec
* **user data** (`set-user`, `user's`): a per-user store, available
  under `system/user`


## The file format

A SAFE 2.0 file is protected by:

* **key derivation**: the password is turned into keys using
  PBKDF2-HMAC-SHA256 (600000 iterations by default) with a random salt,
  so guessing passwords is slow
* **encryption**: `chacha20` (or `aes-256-gcm` when ChaCha20 is not
  available) with a random IV for every write
* **integrity**: HMAC-SHA256 over the whole file, checked before anything
  is decrypted, so a tampered or corrupted file is rejected

The cipher and the number of iterations are stored in each file, so files
stay readable when the defaults change. Files in the older SAFE 1.0
format can still be read; they are written in the 2.0 format on the next
save.

Any data can be stored, as long as it is [serializable](#serializable-values).
Binary data are stored as they are; other values are stored using
`mold/all` and compressed.


## Passwords

When a password is needed and none was given, SAFE uses:

1. the `REBOL_SAFE_PASS` environment variable, when it is set and not empty
2. otherwise it asks in the console (twice, when new data are created)

Cancelling the prompt (Escape) stops the operation with an error.

Note: `REBOL_SAFE_PASS` is convenient for unattended scripts, but anyone
who can read the process environment can read the password.


## Using the codec

Encrypt any value to a binary and decrypt it back:

```rebol
bin: system/codecs/safe/encode/key [user: "oldes" token: "abc123"] "my password"
system/codecs/safe/decode/key bin "my password"
;== [user: "oldes" token: "abc123"]
```

With a wrong password (or a damaged file), `decode` returns `none`.

The usual `save` and `load` work with the `.safe` suffix too; the
password is then taken from `REBOL_SAFE_PASS` or asked for:

```rebol
save %secrets.safe #[token: "abc123"]
load %secrets.safe
```

Codec refinements:

* `encode/key password`, `decode/key password`: the password to use
  (`/key none` means no password: `decode` returns `none`, `encode`
  raises an error, and neither asks)
* `encode/as cipher`: one of the allowed ciphers (`chacha20`, `aes-256-gcm`)
* `encode/iterations count`: the PBKDF2 iteration count (default 600000)
* `encode/cache keys`, `decode/cache keys`: a block used to cache the
  derived keys, so the slow key derivation runs only once per salt
  (used by the scheme)


## Using the scheme

The `safe` scheme keeps a map of values in an encrypted file. Changes are
made in memory and written to the file by `update` or `close`.

```rebol
db: open [scheme: 'safe pass: "my password" path: %/home/oldes/ target: %notes]
put db 'token "abc123"
select db 'token
;== "abc123"
update db   ;; writes the changes
close db    ;; writes any remaining changes and releases the password
```

Spec fields:

* `path`: directory of the file (default: `system/options/home`)
* `target`: file name; `.safe` is added when missing (default: `%.safe`)
* `pass`: the password; when missing, `REBOL_SAFE_PASS` or a prompt is
  used (confirmed when the file is new)
* `host`: reads the file from `https://host/path/target`; saving to a
  URL is not supported

When the file doesn't exist, it is created. Opening fails with an error
when the password is wrong or the file is corrupted.

Port actions:

| Action | Description |
|--------|-------------|
| `put db key value`, `poke db key value` | stores a value |
| `select db key`, `pick db key` | returns a copy of a stored value (or `none`) |
| `remove/key db key` | removes a value |
| `update db` | saves local changes, or loads changes made by others |
| `close db` | saves local changes and closes the port |
| `open? db` | returns `true` while the port is open |
| `modify db 'password ["old" "new"]` | changes the password |
| `modify db 'reload true` | drops local changes and loads the file |
| `modify db 'overwrite true` | saves local changes over changes made by others |

`modify db 'password "new"` (with a string) asks for the old password in
the console.

Keys are words. Returned values are copies, so changing a returned series
doesn't change the stored data.

### Serializable values

Only values which can be saved and loaded back can be stored. `put`
raises an error for values which contain a function, a handle, a port or
another system value, or which contain themselves (circular references).
See `serializable?`. This way one unsupported value can never make the
whole file unreadable.

### Sharing a file between processes

Several processes (or several ports) can use the same file safely:

* the file is locked while it is read (shared lock) or written
  (exclusive lock); a lock held by someone else is waited for up to
  `system/schemes/safe/lock-wait` (1 second by default)
* `update` loads changes made by others when there are no local changes
* when the file was changed by someone else and there are also local
  changes, nothing is written; a warning is printed instead:

```
*** File was modified by another process; local changes not saved: %/home/oldes/notes.safe
```

The local changes stay in the port. Resolve the conflict with
`modify db 'reload true` (keep the other changes) or
`modify db 'overwrite true` (keep yours).

Only the `.safe` file itself is accessed, so when Rebol runs with
security enabled (`secure`), only that one file needs permission.


## User data

`set-user` (short: `su`) opens the encrypted store of a user and makes it
available under `system/user`:

* `system/user/name`: the user's name
* `system/user/data`: the open `safe` port

By default the data are stored in `.<name>.safe` in the home directory
(`system/options/home`).

Create a new user (the password is asked for twice):

```rebol
su/n oldes
```

Log in later:

```rebol
su oldes
```

Store and read values:

```rebol
put system/user/data 'github-token "ghp_xxxxx"
user's github-token
;== "ghp_xxxxx"
```

`user's` returns `none` when the value is not stored or when no user is
logged in.

Log out (saves changes, closes the store and releases the password):

```rebol
su
```

`set-user` refinements:

* `/n`: creates the user's file when it doesn't exist (without `/n`, a
  missing file is an error)
* `/p password`: uses the given password instead of `REBOL_SAFE_PASS`
  or the prompt
* `/f file`: uses a custom file instead of the default one

The name can be a word (stored as `@name`), a string or an email.

When opening fails (for example, a wrong password), the currently logged
in user stays logged in.


## Limitations

* **Crash during a write**: the file is written in place. If the process
  crashes during a write, the file is corrupted; the integrity check
  detects it on the next load, but the data are lost. Keep a backup of
  important data.
* **Lost password**: there is no way to recover data without the password.
* **Timestamp resolution**: changes made by others are detected by the
  file's modification time. On file systems with coarse timestamps
  (like FAT, 2 seconds), two writes within that time can't be told apart.
* **Network file systems**: file locks may not work reliably on network
  file systems (like NFS).
* **Password in memory**: while a store is open, its password is kept in
  memory (hidden from scripts, but not protected from tools which can
  read the process memory).
