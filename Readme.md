# CMS OpenFX plugin for Natron and Nuke 

This is a collection of plugin I made to :
* Create 3D luts with a set of 2 plugins (CMSPattern and CMSBakeLut)
* Read Magic Lantern MLV files natively (CMSMLVReader)

[Natron] Just install the package in an OpenFX plugin path (or in [NatronAppDir]/Plugin/OFX/Natron directory)
OR
[All softs supporting openFX] Set the OPENFX_PLUGIN_PATH environment variable to the root path of the plugin

# Dependencies

* libraw (supplied in the source tree, because it needs specific build)
* Eigen3

# Build on Windows

Use standard CMake build and install method with msys2/mingw64

# Build on Linux

I will fix the build system later for linux, but it should be nearly OK, though

# Focus pixel maps

Create a "fpm" directory in the /Content folder of the plugin and install the maps inthere.
