# nvasm-new
In the days before HLSL was mainstream, many old DirectX 8 applications used a program called "nvasm.exe" to compile their assembly shaders. This program was particularly useful, as it allowed for the use of C-style preprocessor macros. Unfortunately, this program is extremely hard to find these days, as NVIDIA have since dropped it from their website, and no archives currently exist (to my knowledge).

While it is difficult to determine the full capabilities of nvasm, one can intuit some of its key functions. This application attempts to reproduce what is (as far as I can tell) its most useful function: compiling shaders that use C preprocessor directives. At the moment, only "include" and "define" are supported. Besides these two, I'm not really sure what the extent of nvasm's support was.

By default, the compiled shader is output in a binary format. However, if the "-h" option is specified, it will instead be output as a header than can be simply included in your project. I'm sure nvasm supported other options, but the project I made this for didn't use them, so I'm not sure what they are or what they would do.

# Dependencies
While no special prerequisites are required to _compile_ this application, the DirectX 8 shader assemblers "vsa.exe" and "psa.exe" are required to actually use it successfully. Luckily, unlike nvasm, these applications are still available from sufficiently old versions of the DirectX 8 SDK.

# Usage
The latest Windows binary for nvasm-new can be downloaded in the releases section. The application may be used as follows:

`nvasm.exe -[OPTIONS] "shader_path" "output_path"`

While the shader path is required, the output path is not: if none is specified, it will use whatever vsa or psa output by default.

Currently, the valid options are:

| Option | Description |
| --- | --- |
| h | Output the shader as a header file. |