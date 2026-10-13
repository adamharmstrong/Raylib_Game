# Runtime dependencies

Lua 5.4.8 is vendored from https://www.lua.org/ftp/lua-5.4.8.tar.gz.
Its MIT license is included in `lua-5.4.8/doc/readme.html` and source headers.
CMake builds the interpreter as a static library; no system Lua installation
or runtime download is required.

nlohmann/json 3.11.3 is the unmodified single header from
https://github.com/nlohmann/json/releases/tag/v3.11.3. Its MIT license is
included in `nlohmann/LICENSE.MIT`. It decodes attachment path arrays, including
Unicode escapes emitted by the editor.
