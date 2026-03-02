# Pair enhancements

## Why the subpixel resolution is needed?
 * REBOL display engine uses AGG, which supports positions precise to 1 / 256 pixel, therefore it is not reasonable to limit ourselves to less precise input, than the graphic engine already supports
 * For scaling, we need higher precision, than what we can display, since if we rescale a picture doubling its size, former difference of 0.5 pixel becomes 1 pixel difference
 * When displaying the picture on a different medium than the display, higher resolution is needed for the picture to look good
 * Current R3 GUI resizing already uses subpixel pair resolutions to maintain higher accuracy in some cases

## How to implement pairs having subpixel resolution?

### The alternatives that were considered

These were proposed as possible alternatives, before the FP pair variant prevailed: 

#### Using the integer pair implementation

It may be a surprise for many, but, actually, we don't even have to change the implementation of REBOL pair to achieve subpixel accuracy. The only thing we have to do is define, how REBOL pairs are interpreted. If we state, that REBOL pairs are interpreted as dimensions in subpixels, where every subpixel is e.g. a 1 / 65536 of a pixel (later on I shall explain, why this "magic number" looks reasonable to me), then we are done, no other pair implementation change is totally necessary.

##### How to load, mold, form pairs with subpixel accuracy?

To support the subpixel accuracy we can adjust the LOAD, MOLD, FORM functions to take the subpixel interpretation into account. This is how we can modify the LOAD function to accept two decimal coordinates X and Y for a pair:

```rebol
; this defines the interpretation (accuracy) of the pair:
subpixel-count: 65536
load-pair: func [
	{convert decimal coordinates to subpixels}
	x [number!]
	y [number!]
] [
	return (x * subpixel-count + 0.5 * 1x0) + (y * subpixel-count + 0.5 * 0x1)
]
mold-pair: func [
	{mold a subpixel accuracy pair to a "human readable" form}
	s [pair!] {a subpixel-accuracy pair}
] [
	rejoin [round/to s/x / subpixel-count 0.000015 "x" round/to s/y / subpixel-count 0.000015]
]
```

```rebol
;example usage
; a 123.45x678.90 pair
load-pair 123.45 678.90 ; == 8090419x44492390 (this would be the "internal representation")
mold-pair 8090419x44492390 ; == "123.45x678.9"
```

##### Why 1 / 65536 pixel looks like a reasonable accuracy?

As stated above, REBOL graphic engine can already handle 1 / 256 of a pixel, i.e. 8 bits of subpixel accuracy. To have some reserve for other purposes, like scaling, displaying on other devices, it is useful to have a reserve, which is enabled by the use of 16 bits of subpixel accuracy.

#### How to implement REBOL pair keeping the "original" 32-bit integer range?

Currently in R3, as well as in R2, all 32 bits are used for "pixel-and-above" accuracy. To get the same range while devoting 16 bits for subpixel accuracy we would need 48 bits per coordinate, i.e. 96 bits in total for the pair, which seems to be perfectly "in reach" of the current REBOL value implementation. Similarly as now, both coordinates would be integers, just 48-bit. They would be interpreted as coordinates given in 1 / 65536 of a pixel, i.e. in subpixels exactly as above.

### REBOL pair implemented as a pair of floats

This is the alternative, that was chosen, finally. REBOL pairs were implemented as pairs of 32-bit IEEE 754 floats, which gives us 24-bit accuracy in total for every coordinate.

#### Properties

Due to the fact, that AGG actually needs 64-bit IEEE 754 (decimal!) coordinates, and a pair of decimals does not fit into one REBOL value slot currently, the next value fastest to convert to 64-bit IEEE 754 value is a 32-bit IEEE 754 value. (Integer subpixel values would need a floating point multiplication, which would be slower.)

The maximal integer coordinate expressible with at least a one pixel precision would be 16777216. (2 \*\* 24)

### Pair Actions and Functions

 * make
 * to

Binary actions between pair! and also with number! values:

 * add
 * subtract
 * multiply
 * divide
 * remainder
 * comparison (equal, etc.)

Binary functions:

 * minimum
 * maximum

Unary actions:

 * odd
 * even
 * negate
 * complement
 * absolute
 * reverse
 * random

Indexing:

 * pick
 * skip
 * at
 * change
 * copy
 * index?
 * insert
 * remove
 * take

#### Changes to pair actions

The COMPLEMENT function is not supported for REBOL decimals. Since any of these changes would make pair coordinates more similar to decimals, it would lose meaning to keep the support for computing pair complements.

Retrieved from "[http://www.rebol.net/wiki/Pair_enhancements](https://web.archive.org/web/20130614002756/http://www.rebol.net/wiki/Pair_enhancements)"