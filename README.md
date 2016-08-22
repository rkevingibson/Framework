# Framework
Some general framework code that I use to write OpenGL programs more quickly. 
Includes a multi-threaded wrapper for OpenGL 4ish, using the key-sorting technique as outlined [here, by c0de517e][http://c0de517e.blogspot.ca/2014/04/how-to-make-rendering-engine.html], and implemented by Branimir Karadžić in the lovely [bgfx library][https://github.com/bkaradzic/bgfx]. 

I also have a simple file for using Omar Cornut's [Dear Imgui][https://github.com/ocornut/imgui] library, which I include as a submodule and cannot recommend highly enough. It's fantastic.

I use the [GLFW library][http://www.glfw.org/] for window and input management, at least for now. I include the binaries with this repo for Visual Studio 2015, since that is what I use.

This will likely continue to grow to include things resembling a basic game engine - I got sick of copying/pasting code from various research projects and finally decided to put them in one place. Things I expect to be added include:
* Basic mesh loading (ply, obj files at the least)
* Basic texture loading (probably using stb libraries)
* Virtual camera/arcball implementations
* Some geometry code

