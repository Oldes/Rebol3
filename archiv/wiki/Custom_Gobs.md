# Custom Gobs

This is a proposal based on tests and a prototype in development which allows external graphic rendering engines to integrate directly within R3 view system.

It may be referred to as the CGR (Custom Gob Renderer) API within the body of this document.

We aim to expand the gob API. As some ideas get implemented or revised, this document will serve as a basis for its documentation.

#### Why?

Its not because we use an alternate graphic engine that we want to relinquish the use of View's easy to use graphic engine.

This is even more true when we want to use a \*REBOL\* GUI package and just embed some other graphic system into the same window.

#### A quick note about the R3 rendering pipeline

The R3 view has basically 3 levels of graphic management, only one which is directly accessible from within the interpreter.

 * Gob! - The purely Rebol Hi-level interface which is like a handle to the internals.
 * Compositor - Although not a clear-cut layer in and of itself, it really does act as middle layer in the view system. It reads the gob information tree and manages what is rendered and how to layer them, eventually forking off to lower-level AGG routines.
 * AGG - Anti Grain Geometry engine - is the low-level view processing workhorse.  Its a relatively fast and flexible CPU-based 2D vectorial rendering engine.

This proposal integrates an API within the gob! system which is detected and managed by the compositing engine.

The compositor is where the raster creation is launched within view's graphics pipeline and the compositor knows about things like visibility, sizing, and transparency information.

The compositor also scans the gob in order to know what rendering process to call internally, so its a logical place to fork to external renderers.

#### A quick note about the prototype in development

The prototype is not using some of the features in this proposal because parts of view are closed since they require management from the r3 core (its a datatype! after all).

The prototype already addresses changes in the compositor and uses an alternative (slow) method to manage gob-relative custom data.

The current focus is mainly in testing and identifying strengths and weaknesses in the current API in order to make it more flexible, at minimal cost in RAM, speed and man-hours.

#### Rendering Speed considerations

Using any graphic engine WITHIN another obviously comes at a price and this price is speed.

The prototype is the test bed in which we will take decisions in order to achieve acceptable performance.  Based on empiric testing, we may provide switches in the system to increase refresh rates at the cost of some capabilities.  For example, some rendering engines can render directly within an OS window.  some possible side-effect is that allowing this will prevent AGG from drawing over the custom gob.  In such a case, and if no alternative is possible, as switch will be added [EDIT: see [render modes](#OS-Level_integration_of_view_and_custom_Renderers)] to let you decide if you need raw-performance, or flexibility.

The goal is to be able to reach very high refresh rates (100fps is a minimum) at very high screen resolutions (1440x900 minimum), basically, allowing effective real-time graphics for apps such as video editing, video-playback, games, and other high-throughput applications.

The system must not impede the advantages of a rendering engine by slowing it down at the display stage.

#### What rendering engines are we targeting?

The list is short but covers a wide range of requirements

 * **OpenGL**: The most widely used hardware-able gfx lib in the world([http://www.opengl.org](https://web.archive.org/web/20130614002948/http://www.opengl.org/)).
 * **DirectX**: The fastest and most integrated hardware-able gfx API on windows, it also has many productive gaming libs ([http://www.microsoft.com/games/en-us/aboutgfw/pages/directx.aspx](https://web.archive.org/web/20130614002948/http://www.microsoft.com/games/en-us/aboutgfw/pages/directx.aspx)).
 * **Cairo**: has a very active community, is used by many applications, OSes and toolsets.  It has many nice integrated features.([http://cairographics.org/](https://web.archive.org/web/20130614002948/http://cairographics.org/))
 * **Image Magick**: Image magick is used by many high-end softwares and is probably one of the oldest graphic rendering packages in the world.  It started as an SGI project.  It allows rendering at any bit depth, supports HDRI natively and allows MASSIVE direct to disk rendering (tens of GBs of data per render is easily managed).  It has a wide range of effects, supports just about all image formats known to man. ([http://www.imagemagick.org/script/index.php](https://web.archive.org/web/20130614002948/http://www.imagemagick.org/script/index.php))
 * **WebKit's WebCore or Gecko** used by many browsers, it would allow embedding standards' compliant web content directly within view applications with minimal effort.([http://webkit.org/](https://web.archive.org/web/20130614002948/http://webkit.org/))

# The proposed custom GOB API at a glance

## Creating a custom GOB renderer

This is done via the host-kit/extension API requiring you to bind external C/C++ rendering code to the custom GOB API.

Basically, in the Compiled Extension code you:

 * Define a struct (referred in this document as **custom_gob_data**) which will hold renderer data for each gob instances.  You must store this struct within the **rebol_gob->custom** member. 
 * Create and setup a **gob_renderer** struct so it holds callbacks for your external renderer, allowing the internal engine to execute your own code. it has the following hooks:

 * **on_custom()/on_internal()** allows you to build or destroy your custom_gob_data struct, and link it to the R3 allocated **rebol_gob**.  This is also where you handle any rendering engine needs.
 * **on_append()/on_remove()** Called when a gob is about to be added/removed to another gob.  
 * **on_data()** given user data in Rebol Extension format, this will usually result in setting up native (Binary) data within **rebol_gob->custom**.  The information is actually stored in usual gob fashion, within **rebol_gob->data**.
 * **on_render()** (re)generate the custom's gob image.  You are given an RGBA buffer and rectangle area information.  If your gob would be clipped from the display, it is not even called for rendering. A member within the **rebol_gob** struct already indicates if your renderer should blend itself using alpha channel masks to optimize rendering when its not required.

 * **RegisterGobRenderer()** supplying your gob_renderer_hooks struct and a string that will be usable as a word within Rebol.

## Using custom gobs

If you require a gob to be managed by an external renderer, use the renderer: field.

```rebol
   my-gob: make gob! [
       size: 100x100 
       offset: 0x0 
       renderer: 'MOAGL 
       custom: [sphere [0 0 0] 30.0 blue]
   ]

```

This will invoke the appropriate renderer on it.

The reason we use CUSTOM: instead of DATA: is that higher-level APIs (ex: R3 GUI ) will need the DATA: field for their own data storage.

### Default Custom Renderers

You can also use the 'SET-DEFAULT-CUSTOM-RENDERER function.  This prevents you from having to set the RENDERER: field all the time. You must still use the CUSTOM: field in order to tell view that this gob uses the default renderer.

```rebol
   set-default-custom-renderer: 'MOAGL 

   my-gob: make gob! [
       size: 100x100 
       offset: 0x0 
       custom: [sphere [0 0 0] 30.0 blue]
   ]

```

# Changes required in current Rebol version (A107) to make it work
```rebol
   Note: This is in a constent state of flux and for now is only intended as an example
         of what to expect, eventually.

```

Here is a list of files which will need editing.  I include some of the changes I can already \*guess\* will be needed.

Note that these are still highly subjective assessments at this point.  Not everything here might be possible or feasible, based on knowledge which is kept within the core.

Some of these have already been applied to the prototype and are undergoing testing right now.

## host_graphics.c

 * Add a linked list or array in order to stored registered External Renderers
 * Define RegisterGobRenderer()
 * Define GetGobRender()
 * Define macros to call External renderer hooks

## r3core.lib

 * Call hooks on custom gobs at appropriate low-frequency moments (creation/destruction/linking, etc)

Basically, when the interpreter is managing the gob! It should allow a callback within the hostkit.
Additionally, we could even support this for AGG rendering, allowing nice tricks using compiled code to complement the internal rendering (filters, codecs, etc).

 * support gob/custom: [... data ...] ; ANY legal host-kit data
 * support gob/renderer: 'rendererID ; a word, ex: 'OpenGL

## reb_gob.h

 * Add **custom** member to **rebol_gob struct**
 * add a new entry in the **GOB_TYPES** enum:  **GOBT_CUSTOM**
 * define IS_GOB_CUSTOM(g)   (GOB_CONTENT(g) && GOB_TYPE(g) == GOBT_CUSTOM)
 * define the **gob_renderer** struct something like (its changing daily and is not up to date):

```C
   struct gob_renderer {
       //*************** 
       //     payload
       //*************** 
       REBYTE *renderer_info; // a one-sentence string identifying this renderer

       //*************** 
       //     hooks
       //*************** 

       // when the gob is given fresh data by assigning to its <b>gob/custom:</b> field
       REBINT (* <u><i>on_data</i></u>)(REBGOB *gob);

       // when the gob is converted to a custom type (custom: [... data ...])
       // note: also called when its created as a custom gob directly
       REBINT (* <u><i>on_custom</i></u>)(REBGOB *gob);

       // when the gob leaves custom state.  This happens when you assign
       // user data to the gob other than custom: data (ex:  my_custom_gob/draw: [line 0x0 100x100])
       REBINT (* <u><i>on_internal</i></u>)(REBGOB *gob);

       // when gob is appended to a gob (pane) 
       // note: converting to custom when its already appended, should also call this immediately after on_internal().
       REBINT (* <u><i>on_append</i></u>)(REBGOB *gob, REBGOB *parent);  

       // when removed from a gob (pane)
       // note: when an gob is switched from one pane to another, it should call 
       REBINT (* <u><i>on_remove</i></u>)(REBGOB *gob, REBGOB *parent);  on_append() immediately.

       // when rendering is required by compositor and GOBT_EXTERNAL is set. 
       // a function which receives an RGB (bgr) & alpha image buffer array (not image type), and x,y,w,h, specs.
       REBINT (* <u><i>on_render</i></u>)(REBGOB *gob); 
   };

```

## host_window.c

 * Some Windowing functions, such as **Find_window()**, are required by some gfx engines in order for us to retrieve the windowing/screen environment from the OS (OpenGL needs the window DC, for example).

## host_window.h(\*new)

 * Add any externalized host_window.c functions here.

## agg_compo.cpp

 * Adding a case within **compositor::cp_process_gobs()** to support the GOBT_CUSTOM gob type.

```C
   case GOBT_CUSTOM:
       // call rendering hook here
       // there are a lot of tests to do before proposing an explicit
       // code example
       break;

```

# Notes and Considerations

Here we store comments, questions, ideas, bugs, quirks or issues raised so we don't forget them further down the road.

## Questions:

### Unavailable Renderers

**Q:** What do we do when the on_render() hook returns a negative response?

Its possible the HW isn't ready to render for example, the application timer might be going faster than the gfx card can respond on a complex scene).

**A:** Based on the nature of the response, we could do one of a few things, this could be a setup within the initialization of a rendering engine:
-nothing (ignore the renderer for this refresh, don't trample current window buffer)
-insert last render
-a text message with an error
-a fallback AGG drawing.
-raise a Rebol error.

### Managing transparency for event purposes

**Q:** In case of overlapping rendering regions provided by multiple renderers - would interaction events (mouse, touch, etc) be passed through to the underlying renderer if the hit ocurred in a zone that was 'transparent' (according to some key color provided to the compositor)? It would be quite useful to have this.

**A:** Event processing is not the task of the renderer, BUT determining what lies under a rectangle offset can only be done quickly by the renderer.  So we could add a hook to let renderers tell us if they see something at given coordinates (like the mouse pointer).

In GLASS the rendering engine creates a "backplane" layer which is a color bitmask used to detect what object is concerned with a specific region of the display.

This is rendered by the objects themselves, so transparency and non-rectangular regions are supported by default. It is only re-rendered when some object needs to, so unless the shape or sizing changes, its never actually redrawn (saving a lot of processing).

Lookup is VERY fast since its a simple pick within a series (in this case an image!).

I can see us re-using this concept in VIEW also and AFAIK that is what they are doing for R3 GUI.  We could easily provide a hook for a rendering engine to provide its own region mask.

Alternatively, a hook could be added which returns an Integer identifying an object is some way.  Depending on the renderer this could be more or less work than rendering a bit mask.  Some, Like OpenGL and WebKit, have an integrated way to return a object number or pointer based on a pixel position of the viewport.  

When this returns none, the view engine, could just verify the gob under it, until it hits the window gob.

Although this sounds like a lot of processing, when compared to a single refresh of a full display, its really nothing.

## OS-Level integration of view and custom Renderers

After a lot of testing, searching and looking around for some solutions... there are some serious limitations to the way I can integrate OpenGL (and other rendering pipelines) within view, without turning them into really slow implementations of their former selves.

The System level graphical implementations for most or all platforms have various levels of oomph and some just can't cope with the most basic issue of blending bitmaps using alpha channels directly within their API.  Some do at a high cost, or with strange side-effects.

So this leads me to have to decide on an integration path.  Ultimately, I'd like to allow several methods, which will cater to various needs, but for now, its pretty obvious that each alternative will require pretty different setups and integrations within the view engine or the custom gob API.

This is even more complicated since view allows rendering to bitmaps directly, for which some rendering engines might not even be able to provide any level of support.

### Rendering Modes

For the above reasons, I've added explicit management over chosen rendering methods within CGR API.

Most of this is invisible to the user a part perhaps the fact that some modes will simply be ignored by some renderers, which will result in basically hidden gobs in the tree.

I list here The different rendering methods which could (will?) be supported within CGR API.
For OpenGL support, I will be starting with the container method since its what is basically used by everyone out there.

### CONTAINER(relatively easy, most flexible, fast)

we tell The renderer Where to render, but it does not actually integrate within the view/AGG gfx pipeline.

##### Pros:

 * We use gobs to allocate & integrate the renderer within our view (placement, sizing, user data, refresh calls, etc).
 * Rendering can be performed in HW with no interference from view, so its Optimally fast.
 * gob children could be used by the custom renderer instead (ex, an image gob could be used as a texture).

##### Cons:

 * It cannot be drawn over by view.
 * The rendering might (most probably) not allow alpha-blending of its content (i.e. bg is always opaque).
 * view-based gob children would be ignored by view for sake of speed (no sense handling them if AGG can't use them).

##### unknowns:

 * its possible we could have (internal gob) children of the renderer, within a new container.
 * in this case support for alpha-blending is unknown, but probably will slow down processing if possible.
 * would probably require an extension to R/view because that means allocating a new container for the gob (ex: a new child window).

### COMPOSITE (easy, most integrated, slowest)

100% integration within the gob gfx pipeline.  

##### Pros:

 * mininal work wrt view,
 * Rendering can be done in HW using off-screen rendering, on relatively old gfx cards (within 5-7 years old) 
 * we \*can\* draw under and over other gobs, as well as support alpha blending as normal in view.
 * this allows people to integrate custom graphic algorithms using VERY little C code to plug into view.

##### Cons:

 * Its going to be \*very\* slow on large displays anyways because we have to use CPU blitting at the end.
 * Older HW might have to use CPU rendering (even slower i.e. unacceptible for animation).

##### Notes:

 * Most of it is already working and functional.
 * All interfacing with the renderer is done via a memory buffer, so ANY engine can be supported this way.

### BITMAP(a subset of COMPOSITE)

100% integration within the gob gfx pipeline, rendered "off-screen".

##### Pros:

 * similar to composite mode, but doesn't expect any performance, since this is meant to generate images.

##### Cons:

 * similar to composite mode, but not all renderers will support off-screen rendering, because they might depend on windowing functions, buffers or environements.

##### Notes:

 * same as composite mode

### HARDWARE (fastest, hardest, least stable)

This will not be developed for a while, if ever.  it requires re-working of the view internals so THEY
are mapped into the HW as a texture.  its also the least Platform independent.

The net result is a simple boost in speed, since the CPU is mostly out of the equation at this point.  Some processes, like building textures and compositing panes might still be performed at this level, but it would depend on the possibility of also integrating AGG at this level.

One difficulty, IMHO, is in re-working the compositor using layers of blended HW textures instead CPU pixel blitting.

##### Pros:

 * Fastest of any solution.
 * Pure hardware mode, makes it optimal for games development.
 * View panes effectively can be used using real Z depth and the gob order could be re-organized visually, instead of being hard-computed.

##### Cons:

 * Makes view Dependent on gfx capabilitities of the HW
 * intermediate rendering required for clipping is complex to deal with in 3D since it implies rendering textures on the fly.
 * Blending in 3D can be complex and some artifacts are possible in some circumstances and on some cards.
 * Pixel precision is subject to gfx HW implementation and drivers.
 * Will not allow view on old/server hardware.

##### Notes:

 * Basically requires rebuilding the view architecture so that the renderer is at the heart of the system and view/agg plugs into it (this is a lot of work and not worth it in the short term).
 * Its possible, others are already working to support this to some level, so I'll let it be for now. If such a setup springs up, I'll integrate into it as is possible.

Retrieved from "[http://www.rebol.net/wiki/Custom_Gobs](https://web.archive.org/web/20130614002948/http://www.rebol.net/wiki/Custom_Gobs)"