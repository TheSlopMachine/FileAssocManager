#include "formats.h"

#define F(e, d, c) { L##e, L##d, c }

const FmtDef FMT_DEFS[] = {
    /* ------------------------------------------------------------- images */
    F(".jpg", "JPEG image", CAT_IMAGES), F(".jpeg", "JPEG image", CAT_IMAGES), F(".jpe", "JPEG image", CAT_IMAGES),
    F(".jfif", "JPEG image (JFIF)", CAT_IMAGES), F(".jif", "JPEG image", CAT_IMAGES),
    F(".png", "PNG image", CAT_IMAGES), F(".apng", "Animated PNG", CAT_IMAGES),
    F(".gif", "GIF image", CAT_IMAGES), F(".bmp", "Bitmap image", CAT_IMAGES), F(".dib", "Device-independent bitmap", CAT_IMAGES),
    F(".tif", "TIFF image", CAT_IMAGES), F(".tiff", "TIFF image", CAT_IMAGES),
    F(".webp", "WebP image", CAT_IMAGES), F(".heic", "HEIC image", CAT_IMAGES), F(".heif", "HEIF image", CAT_IMAGES),
    F(".avif", "AVIF image", CAT_IMAGES), F(".jxl", "JPEG XL image", CAT_IMAGES),
    F(".ico", "Icon", CAT_IMAGES), F(".cur", "Cursor", CAT_IMAGES), F(".ani", "Animated cursor", CAT_IMAGES),
    F(".svg", "SVG vector image", CAT_IMAGES), F(".svgz", "Compressed SVG image", CAT_IMAGES),
    F(".jp2", "JPEG 2000 image", CAT_IMAGES), F(".j2k", "JPEG 2000 image", CAT_IMAGES),
    F(".jpf", "JPEG 2000 image", CAT_IMAGES), F(".jpx", "JPEG 2000 image", CAT_IMAGES),
    F(".tga", "Targa image", CAT_IMAGES), F(".pcx", "PCX image", CAT_IMAGES),
    F(".pbm", "Portable bitmap", CAT_IMAGES), F(".pgm", "Portable graymap", CAT_IMAGES),
    F(".ppm", "Portable pixmap", CAT_IMAGES), F(".pnm", "Portable anymap", CAT_IMAGES), F(".pam", "Portable arbitrary map", CAT_IMAGES),
    F(".hdr", "Radiance HDR image", CAT_IMAGES), F(".exr", "OpenEXR image", CAT_IMAGES), F(".dds", "DirectDraw surface", CAT_IMAGES),
    F(".psd", "Photoshop document", CAT_IMAGES), F(".psb", "Photoshop large document", CAT_IMAGES),
    F(".xcf", "GIMP image", CAT_IMAGES), F(".kra", "Krita document", CAT_IMAGES), F(".ora", "OpenRaster image", CAT_IMAGES),
    F(".ai", "Adobe Illustrator artwork", CAT_IMAGES), F(".eps", "Encapsulated PostScript", CAT_IMAGES),
    F(".emf", "Enhanced metafile", CAT_IMAGES), F(".wmf", "Windows metafile", CAT_IMAGES), F(".wbmp", "Wireless bitmap", CAT_IMAGES),
    F(".qoi", "QOI image", CAT_IMAGES), F(".sgi", "SGI image", CAT_IMAGES), F(".ras", "Sun raster image", CAT_IMAGES),
    F(".xbm", "X bitmap", CAT_IMAGES), F(".xpm", "X pixmap", CAT_IMAGES), F(".dpx", "DPX image", CAT_IMAGES),

    /* --------------------------------------------------------- camera RAW */
    F(".cr2", "Canon RAW", CAT_RAW), F(".cr3", "Canon RAW 3", CAT_RAW), F(".crw", "Canon RAW (old)", CAT_RAW),
    F(".nef", "Nikon RAW", CAT_RAW), F(".nrw", "Nikon RAW (compact)", CAT_RAW),
    F(".arw", "Sony RAW", CAT_RAW), F(".sr2", "Sony RAW", CAT_RAW), F(".srf", "Sony RAW", CAT_RAW),
    F(".orf", "Olympus RAW", CAT_RAW), F(".rw2", "Panasonic RAW", CAT_RAW), F(".raf", "Fujifilm RAW", CAT_RAW),
    F(".dng", "Digital Negative", CAT_RAW), F(".pef", "Pentax RAW", CAT_RAW), F(".srw", "Samsung RAW", CAT_RAW),
    F(".x3f", "Sigma RAW", CAT_RAW), F(".3fr", "Hasselblad RAW", CAT_RAW), F(".mrw", "Minolta RAW", CAT_RAW),
    F(".erf", "Epson RAW", CAT_RAW), F(".kdc", "Kodak RAW", CAT_RAW), F(".dcr", "Kodak RAW", CAT_RAW),
    F(".mef", "Mamiya RAW", CAT_RAW), F(".mos", "Leaf RAW", CAT_RAW), F(".rwl", "Leica RAW", CAT_RAW),
    F(".iiq", "Phase One RAW", CAT_RAW), F(".gpr", "GoPro RAW", CAT_RAW), F(".raw", "RAW image", CAT_RAW),

    /* -------------------------------------------------------------- video */
    F(".mp4", "MP4 video", CAT_VIDEO), F(".m4v", "MPEG-4 video", CAT_VIDEO), F(".mkv", "Matroska video", CAT_VIDEO),
    F(".webm", "WebM video", CAT_VIDEO), F(".avi", "AVI video", CAT_VIDEO), F(".mov", "QuickTime movie", CAT_VIDEO),
    F(".qt", "QuickTime movie", CAT_VIDEO), F(".wmv", "Windows Media video", CAT_VIDEO), F(".asf", "Advanced Systems Format", CAT_VIDEO),
    F(".flv", "Flash video", CAT_VIDEO), F(".f4v", "Flash MP4 video", CAT_VIDEO),
    F(".mpg", "MPEG video", CAT_VIDEO), F(".mpeg", "MPEG video", CAT_VIDEO), F(".mpe", "MPEG video", CAT_VIDEO),
    F(".mpv", "MPEG video", CAT_VIDEO), F(".m2v", "MPEG-2 video", CAT_VIDEO), F(".m2ts", "Blu-ray MPEG-2 transport stream", CAT_VIDEO),
    F(".mts", "AVCHD video", CAT_VIDEO), F(".vob", "DVD video object", CAT_VIDEO),
    F(".3gp", "3GPP video", CAT_VIDEO), F(".3g2", "3GPP2 video", CAT_VIDEO),
    F(".ogv", "Ogg video", CAT_VIDEO), F(".ogm", "Ogg media", CAT_VIDEO), F(".divx", "DivX video", CAT_VIDEO),
    F(".rm", "RealMedia", CAT_VIDEO), F(".rmvb", "RealMedia variable bitrate", CAT_VIDEO),
    F(".mxf", "Material Exchange Format", CAT_VIDEO), F(".dv", "DV video", CAT_VIDEO), F(".amv", "AMV video", CAT_VIDEO),
    F(".ivf", "IVF video", CAT_VIDEO),
    F(".srt", "SubRip subtitles", CAT_VIDEO), F(".ass", "Advanced SubStation subtitles", CAT_VIDEO),
    F(".ssa", "SubStation Alpha subtitles", CAT_VIDEO), F(".vtt", "WebVTT subtitles", CAT_VIDEO),
    F(".sub", "Subtitles", CAT_VIDEO), F(".idx", "VobSub subtitle index", CAT_VIDEO), F(".sup", "Bitmap subtitles", CAT_VIDEO),

    /* -------------------------------------------------------------- audio */
    F(".mp3", "MP3 audio", CAT_AUDIO), F(".wav", "Waveform audio", CAT_AUDIO), F(".wave", "Waveform audio", CAT_AUDIO),
    F(".flac", "FLAC audio", CAT_AUDIO), F(".ogg", "Ogg Vorbis audio", CAT_AUDIO), F(".oga", "Ogg audio", CAT_AUDIO),
    F(".opus", "Opus audio", CAT_AUDIO), F(".m4a", "MPEG-4 audio", CAT_AUDIO), F(".m4b", "MPEG-4 audiobook", CAT_AUDIO),
    F(".m4r", "iPhone ringtone", CAT_AUDIO), F(".aac", "AAC audio", CAT_AUDIO), F(".wma", "Windows Media audio", CAT_AUDIO),
    F(".aif", "AIFF audio", CAT_AUDIO), F(".aiff", "AIFF audio", CAT_AUDIO), F(".aifc", "AIFF-C audio", CAT_AUDIO),
    F(".ape", "Monkey's Audio", CAT_AUDIO), F(".wv", "WavPack audio", CAT_AUDIO), F(".mka", "Matroska audio", CAT_AUDIO),
    F(".mpc", "Musepack audio", CAT_AUDIO), F(".mp2", "MPEG audio layer 2", CAT_AUDIO),
    F(".mid", "MIDI sequence", CAT_AUDIO), F(".midi", "MIDI sequence", CAT_AUDIO), F(".rmi", "RIFF MIDI", CAT_AUDIO), F(".kar", "Karaoke MIDI", CAT_AUDIO),
    F(".amr", "AMR audio", CAT_AUDIO), F(".awb", "AMR wideband audio", CAT_AUDIO), F(".ac3", "Dolby Digital audio", CAT_AUDIO),
    F(".eac3", "Dolby Digital Plus audio", CAT_AUDIO), F(".dts", "DTS audio", CAT_AUDIO), F(".au", "Sun audio", CAT_AUDIO),
    F(".caf", "Core Audio Format", CAT_AUDIO), F(".tta", "True Audio", CAT_AUDIO), F(".dsf", "DSD audio", CAT_AUDIO), F(".weba", "WebM audio", CAT_AUDIO),
    F(".m3u", "Playlist", CAT_AUDIO), F(".m3u8", "UTF-8 playlist", CAT_AUDIO), F(".pls", "Playlist", CAT_AUDIO),
    F(".wpl", "Windows Media playlist", CAT_AUDIO), F(".xspf", "XSPF playlist", CAT_AUDIO), F(".asx", "Windows Media redirector", CAT_AUDIO),
    F(".cue", "CD cue sheet", CAT_AUDIO),

    /* ---------------------------------------------------------- documents */
    F(".doc", "Word 97-2003 document", CAT_DOCS), F(".docx", "Word document", CAT_DOCS), F(".docm", "Word macro-enabled document", CAT_DOCS),
    F(".dot", "Word 97-2003 template", CAT_DOCS), F(".dotx", "Word template", CAT_DOCS), F(".dotm", "Word macro-enabled template", CAT_DOCS),
    F(".rtf", "Rich Text Format", CAT_DOCS), F(".odt", "OpenDocument text", CAT_DOCS), F(".ott", "OpenDocument text template", CAT_DOCS),
    F(".fodt", "Flat OpenDocument text", CAT_DOCS), F(".wps", "Works / WPS document", CAT_DOCS), F(".wpd", "WordPerfect document", CAT_DOCS),
    F(".pages", "Apple Pages document", CAT_DOCS), F(".abw", "AbiWord document", CAT_DOCS),

    /* ------------------------------------------------------- spreadsheets */
    F(".xls", "Excel 97-2003 workbook", CAT_SHEETS), F(".xlsx", "Excel workbook", CAT_SHEETS), F(".xlsm", "Excel macro-enabled workbook", CAT_SHEETS),
    F(".xlsb", "Excel binary workbook", CAT_SHEETS), F(".xlt", "Excel 97-2003 template", CAT_SHEETS), F(".xltx", "Excel template", CAT_SHEETS),
    F(".xltm", "Excel macro-enabled template", CAT_SHEETS), F(".ods", "OpenDocument spreadsheet", CAT_SHEETS),
    F(".ots", "OpenDocument spreadsheet template", CAT_SHEETS), F(".fods", "Flat OpenDocument spreadsheet", CAT_SHEETS),
    F(".csv", "Comma-separated values", CAT_SHEETS), F(".tsv", "Tab-separated values", CAT_SHEETS), F(".numbers", "Apple Numbers spreadsheet", CAT_SHEETS),

    /* -------------------------------------------------------- presentations */
    F(".ppt", "PowerPoint 97-2003 presentation", CAT_SLIDES), F(".pptx", "PowerPoint presentation", CAT_SLIDES),
    F(".pptm", "PowerPoint macro-enabled presentation", CAT_SLIDES), F(".pps", "PowerPoint 97-2003 slide show", CAT_SLIDES),
    F(".ppsx", "PowerPoint slide show", CAT_SLIDES), F(".ppsm", "PowerPoint macro-enabled slide show", CAT_SLIDES),
    F(".pot", "PowerPoint 97-2003 template", CAT_SLIDES), F(".potx", "PowerPoint template", CAT_SLIDES),
    F(".odp", "OpenDocument presentation", CAT_SLIDES), F(".otp", "OpenDocument presentation template", CAT_SLIDES),
    F(".fodp", "Flat OpenDocument presentation", CAT_SLIDES), F(".key", "Apple Keynote presentation", CAT_SLIDES),

    /* ------------------------------------------------------ PDF and e-books */
    F(".pdf", "PDF document", CAT_BOOKS), F(".xps", "XPS document", CAT_BOOKS), F(".oxps", "Open XPS document", CAT_BOOKS),
    F(".epub", "EPUB e-book", CAT_BOOKS), F(".mobi", "Mobipocket e-book", CAT_BOOKS), F(".azw", "Kindle e-book", CAT_BOOKS),
    F(".azw3", "Kindle e-book (KF8)", CAT_BOOKS), F(".kfx", "Kindle e-book (KFX)", CAT_BOOKS), F(".fb2", "FictionBook e-book", CAT_BOOKS),
    F(".djvu", "DjVu document", CAT_BOOKS), F(".djv", "DjVu document", CAT_BOOKS),
    F(".cbz", "Comic book (ZIP)", CAT_BOOKS), F(".cbr", "Comic book (RAR)", CAT_BOOKS), F(".cb7", "Comic book (7z)", CAT_BOOKS),
    F(".cbt", "Comic book (TAR)", CAT_BOOKS), F(".chm", "Compiled HTML help", CAT_BOOKS), F(".lit", "Microsoft Reader e-book", CAT_BOOKS),
    F(".prc", "Palm e-book", CAT_BOOKS), F(".lrf", "Sony e-book", CAT_BOOKS),

    /* -------------------------------------------------------- text and config */
    F(".txt", "Text document", CAT_TEXT), F(".text", "Text document", CAT_TEXT), F(".log", "Log file", CAT_TEXT),
    F(".md", "Markdown document", CAT_TEXT), F(".markdown", "Markdown document", CAT_TEXT), F(".mdown", "Markdown document", CAT_TEXT),
    F(".mkd", "Markdown document", CAT_TEXT), F(".rst", "reStructuredText", CAT_TEXT), F(".adoc", "AsciiDoc document", CAT_TEXT),
    F(".org", "Org-mode document", CAT_TEXT), F(".tex", "LaTeX document", CAT_TEXT), F(".bib", "BibTeX bibliography", CAT_TEXT),
    F(".nfo", "Info file", CAT_TEXT), F(".diz", "Description file", CAT_TEXT),
    F(".ini", "Configuration settings", CAT_TEXT), F(".cfg", "Configuration file", CAT_TEXT), F(".conf", "Configuration file", CAT_TEXT),
    F(".config", "Configuration file", CAT_TEXT), F(".properties", "Properties file", CAT_TEXT), F(".env", "Environment file", CAT_TEXT),
    F(".toml", "TOML file", CAT_TEXT), F(".yaml", "YAML file", CAT_TEXT), F(".yml", "YAML file", CAT_TEXT),
    F(".json", "JSON file", CAT_TEXT), F(".jsonc", "JSON with comments", CAT_TEXT), F(".json5", "JSON5 file", CAT_TEXT),
    F(".jsonl", "JSON Lines file", CAT_TEXT), F(".ndjson", "Newline-delimited JSON", CAT_TEXT),
    F(".xml", "XML document", CAT_TEXT), F(".xsd", "XML schema", CAT_TEXT), F(".xsl", "XSL stylesheet", CAT_TEXT),
    F(".xslt", "XSLT stylesheet", CAT_TEXT), F(".dtd", "Document type definition", CAT_TEXT), F(".plist", "Property list", CAT_TEXT),
    F(".diff", "Diff file", CAT_TEXT), F(".patch", "Patch file", CAT_TEXT), F(".reg", "Registration entries", CAT_TEXT),
    F(".gitignore", "Git ignore list", CAT_TEXT), F(".gitattributes", "Git attributes", CAT_TEXT), F(".editorconfig", "EditorConfig file", CAT_TEXT),

    /* ---------------------------------------------------------- source code */
    F(".c", "C source", CAT_CODE), F(".h", "C/C++ header", CAT_CODE), F(".cc", "C++ source", CAT_CODE), F(".cpp", "C++ source", CAT_CODE),
    F(".cxx", "C++ source", CAT_CODE), F(".hpp", "C++ header", CAT_CODE), F(".hxx", "C++ header", CAT_CODE), F(".hh", "C++ header", CAT_CODE),
    F(".inl", "C++ inline file", CAT_CODE), F(".cs", "C# source", CAT_CODE), F(".vb", "Visual Basic source", CAT_CODE),
    F(".java", "Java source", CAT_CODE), F(".kt", "Kotlin source", CAT_CODE), F(".kts", "Kotlin script", CAT_CODE),
    F(".scala", "Scala source", CAT_CODE), F(".groovy", "Groovy source", CAT_CODE), F(".gradle", "Gradle build script", CAT_CODE),
    F(".py", "Python script", CAT_CODE), F(".pyw", "Python script (windowed)", CAT_CODE), F(".pyi", "Python stub", CAT_CODE),
    F(".ipynb", "Jupyter notebook", CAT_CODE), F(".rb", "Ruby script", CAT_CODE), F(".erb", "Embedded Ruby", CAT_CODE),
    F(".php", "PHP script", CAT_CODE), F(".phtml", "PHP template", CAT_CODE),
    F(".js", "JavaScript file", CAT_CODE), F(".mjs", "JavaScript module", CAT_CODE), F(".cjs", "CommonJS module", CAT_CODE),
    F(".jsx", "React JSX file", CAT_CODE), F(".ts", "TypeScript (or MPEG-TS video)", CAT_CODE), F(".tsx", "TypeScript JSX file", CAT_CODE),
    F(".vue", "Vue component", CAT_CODE), F(".svelte", "Svelte component", CAT_CODE),
    F(".css", "Cascading style sheet", CAT_CODE), F(".scss", "Sass stylesheet", CAT_CODE), F(".sass", "Sass stylesheet", CAT_CODE), F(".less", "Less stylesheet", CAT_CODE),
    F(".go", "Go source", CAT_CODE), F(".rs", "Rust source", CAT_CODE), F(".swift", "Swift source", CAT_CODE),
    F(".m", "Objective-C / MATLAB source", CAT_CODE), F(".mm", "Objective-C++ source", CAT_CODE),
    F(".lua", "Lua script", CAT_CODE), F(".pl", "Perl script", CAT_CODE), F(".pm", "Perl module", CAT_CODE), F(".r", "R script", CAT_CODE),
    F(".jl", "Julia source", CAT_CODE), F(".dart", "Dart source", CAT_CODE),
    F(".sh", "Shell script", CAT_CODE), F(".bash", "Bash script", CAT_CODE), F(".zsh", "Zsh script", CAT_CODE),
    F(".ps1", "PowerShell script", CAT_CODE), F(".psm1", "PowerShell module", CAT_CODE), F(".psd1", "PowerShell data file", CAT_CODE),
    F(".vbs", "VBScript file", CAT_CODE), F(".wsf", "Windows Script file", CAT_CODE),
    F(".sql", "SQL script", CAT_CODE), F(".asm", "Assembly source", CAT_CODE), F(".s", "Assembly source", CAT_CODE),
    F(".f", "Fortran source", CAT_CODE), F(".f90", "Fortran 90 source", CAT_CODE), F(".pas", "Pascal source", CAT_CODE),
    F(".hs", "Haskell source", CAT_CODE), F(".ml", "OCaml source", CAT_CODE), F(".fs", "F# source", CAT_CODE),
    F(".clj", "Clojure source", CAT_CODE), F(".ex", "Elixir source", CAT_CODE), F(".exs", "Elixir script", CAT_CODE),
    F(".erl", "Erlang source", CAT_CODE), F(".nim", "Nim source", CAT_CODE), F(".zig", "Zig source", CAT_CODE),
    F(".cmake", "CMake script", CAT_CODE), F(".mak", "Makefile", CAT_CODE), F(".tf", "Terraform file", CAT_CODE),
    F(".proto", "Protocol Buffers definition", CAT_CODE), F(".graphql", "GraphQL schema", CAT_CODE), F(".sol", "Solidity source", CAT_CODE),
    F(".wat", "WebAssembly text", CAT_CODE),

    /* ------------------------------------------------------------ web pages */
    F(".htm", "HTML document", CAT_WEB), F(".html", "HTML document", CAT_WEB), F(".xhtml", "XHTML document", CAT_WEB),
    F(".xht", "XHTML document", CAT_WEB), F(".shtml", "Server-side HTML", CAT_WEB),
    F(".mht", "Web archive", CAT_WEB), F(".mhtml", "Web archive", CAT_WEB),

    /* ------------------------------------------------------------- archives */
    F(".zip", "ZIP archive", CAT_ARCHIVES), F(".zipx", "Extended ZIP archive", CAT_ARCHIVES), F(".7z", "7-Zip archive", CAT_ARCHIVES),
    F(".rar", "RAR archive", CAT_ARCHIVES), F(".tar", "TAR archive", CAT_ARCHIVES), F(".gz", "Gzip archive", CAT_ARCHIVES),
    F(".tgz", "Gzip TAR archive", CAT_ARCHIVES), F(".bz2", "Bzip2 archive", CAT_ARCHIVES), F(".tbz", "Bzip2 TAR archive", CAT_ARCHIVES),
    F(".tbz2", "Bzip2 TAR archive", CAT_ARCHIVES), F(".xz", "XZ archive", CAT_ARCHIVES), F(".txz", "XZ TAR archive", CAT_ARCHIVES),
    F(".zst", "Zstandard archive", CAT_ARCHIVES), F(".tzst", "Zstandard TAR archive", CAT_ARCHIVES),
    F(".lz", "Lzip archive", CAT_ARCHIVES), F(".lzma", "LZMA archive", CAT_ARCHIVES), F(".lz4", "LZ4 archive", CAT_ARCHIVES),
    F(".z", "Unix compressed archive", CAT_ARCHIVES), F(".cab", "Cabinet archive", CAT_ARCHIVES), F(".arj", "ARJ archive", CAT_ARCHIVES),
    F(".lzh", "LZH archive", CAT_ARCHIVES), F(".lha", "LHA archive", CAT_ARCHIVES), F(".ace", "ACE archive", CAT_ARCHIVES),
    F(".cpio", "CPIO archive", CAT_ARCHIVES), F(".deb", "Debian package", CAT_ARCHIVES), F(".rpm", "RPM package", CAT_ARCHIVES),
    F(".apk", "Android package", CAT_ARCHIVES), F(".jar", "Java archive", CAT_ARCHIVES),

    /* ------------------------------------------------------------ disk images */
    F(".iso", "Disc image", CAT_DISKS), F(".img", "Disk image", CAT_DISKS), F(".nrg", "Nero disc image", CAT_DISKS),
    F(".mdf", "Disc image", CAT_DISKS), F(".mds", "Disc image descriptor", CAT_DISKS), F(".ccd", "CloneCD descriptor", CAT_DISKS),
    F(".vhd", "Virtual hard disk", CAT_DISKS), F(".vhdx", "Virtual hard disk (VHDX)", CAT_DISKS), F(".vmdk", "VMware virtual disk", CAT_DISKS),
    F(".vdi", "VirtualBox disk image", CAT_DISKS), F(".qcow2", "QEMU disk image", CAT_DISKS), F(".dmg", "Apple disk image", CAT_DISKS),
    F(".wim", "Windows imaging file", CAT_DISKS), F(".esd", "Windows electronic software download", CAT_DISKS),

    /* ---------------------------------------------------------------- fonts */
    F(".ttf", "TrueType font", CAT_FONTS), F(".otf", "OpenType font", CAT_FONTS), F(".ttc", "TrueType collection", CAT_FONTS),
    F(".otc", "OpenType collection", CAT_FONTS), F(".woff", "Web font", CAT_FONTS), F(".woff2", "Web font 2", CAT_FONTS),
    F(".eot", "Embedded OpenType font", CAT_FONTS), F(".fon", "Bitmap font", CAT_FONTS), F(".fnt", "Font file", CAT_FONTS),
    F(".pfb", "PostScript font", CAT_FONTS), F(".pfm", "PostScript font metrics", CAT_FONTS),

    /* ------------------------------------------------------------- 3D and CAD */
    F(".stl", "STL 3D model", CAT_3D), F(".obj", "Wavefront 3D model", CAT_3D), F(".fbx", "Autodesk FBX", CAT_3D),
    F(".dae", "COLLADA model", CAT_3D), F(".3ds", "3D Studio model", CAT_3D), F(".blend", "Blender file", CAT_3D),
    F(".gltf", "glTF model", CAT_3D), F(".glb", "Binary glTF model", CAT_3D), F(".step", "STEP CAD model", CAT_3D),
    F(".stp", "STEP CAD model", CAT_3D), F(".iges", "IGES CAD model", CAT_3D), F(".igs", "IGES CAD model", CAT_3D),
    F(".dwg", "AutoCAD drawing", CAT_3D), F(".dxf", "Drawing exchange format", CAT_3D), F(".dwf", "Design web format", CAT_3D),
    F(".skp", "SketchUp model", CAT_3D), F(".3mf", "3D Manufacturing Format", CAT_3D), F(".ply", "Polygon file", CAT_3D),
    F(".max", "3ds Max scene", CAT_3D), F(".c4d", "Cinema 4D scene", CAT_3D), F(".ma", "Maya ASCII scene", CAT_3D),
    F(".mb", "Maya binary scene", CAT_3D), F(".usd", "Universal Scene Description", CAT_3D), F(".usdz", "USDZ package", CAT_3D),
    F(".wrl", "VRML world", CAT_3D), F(".x3d", "X3D scene", CAT_3D), F(".ifc", "Industry Foundation Classes", CAT_3D),
    F(".sldprt", "SolidWorks part", CAT_3D), F(".sldasm", "SolidWorks assembly", CAT_3D),

    /* ------------------------------------------------------------ databases */
    F(".sqlite", "SQLite database", CAT_DATA), F(".sqlite3", "SQLite database", CAT_DATA), F(".db", "Database file", CAT_DATA),
    F(".db3", "SQLite database", CAT_DATA), F(".mdb", "Access 2003 database", CAT_DATA), F(".accdb", "Access database", CAT_DATA),
    F(".dbf", "dBASE table", CAT_DATA), F(".parquet", "Parquet data file", CAT_DATA), F(".feather", "Feather data file", CAT_DATA),
    F(".arrow", "Arrow data file", CAT_DATA), F(".h5", "HDF5 data file", CAT_DATA), F(".hdf5", "HDF5 data file", CAT_DATA),
    F(".rds", "R data file", CAT_DATA), F(".rdata", "R workspace", CAT_DATA),

    /* ------------------------------------------------- email, calendar, contacts */
    F(".eml", "Email message", CAT_MAIL), F(".emlx", "Apple Mail message", CAT_MAIL), F(".msg", "Outlook message", CAT_MAIL),
    F(".mbox", "Mailbox", CAT_MAIL), F(".pst", "Outlook data file", CAT_MAIL), F(".ost", "Outlook offline data file", CAT_MAIL),
    F(".ics", "Calendar event", CAT_MAIL), F(".vcf", "Contact card", CAT_MAIL),
};
const int FMT_DEF_COUNT = (int)(sizeof FMT_DEFS / sizeof FMT_DEFS[0]);

static const wchar_t *CAT_NAMES[CAT_COUNT] = {
    L"Images", L"Camera RAW", L"Video", L"Audio", L"Documents", L"Spreadsheets", L"Presentations",
    L"PDF & e-books", L"Text & config", L"Source code", L"Web pages", L"Archives", L"Disk images",
    L"Fonts", L"3D & CAD", L"Databases", L"Email & calendar", L"Other types",
};
static const wchar_t *CAT_SAMPLES[CAT_COUNT] = {
    L".jpg", L".cr2", L".mp4", L".mp3", L".docx", L".xlsx", L".pptx", L".pdf", L".txt", L".py",
    L".html", L".zip", L".iso", L".ttf", L".stl", L".db", L".eml", L".dat",
};

const wchar_t *cat_name(int cat) { return (cat >= 0 && cat < CAT_COUNT) ? CAT_NAMES[cat] : L""; }

const wchar_t *cat_sample_ext(int cat) { return (cat >= 0 && cat < CAT_COUNT) ? CAT_SAMPLES[cat] : L".dat"; }

const FmtDef *fmt_def_find(const wchar_t *ext)
{
    for (int i = 0; i < FMT_DEF_COUNT; i++)
        if (_wcsicmp(FMT_DEFS[i].ext, ext) == 0) return &FMT_DEFS[i];
    return NULL;
}

/* Programs, shortcuts and system objects. Reassigning these can make Windows unusable. */
BOOL fmt_is_blocked(const wchar_t *ext)
{
    static const wchar_t *B[] = {
        L".exe", L".com", L".scr", L".pif", L".dll", L".sys", L".drv", L".ocx", L".cpl", L".msc", L".msi",
        L".msp", L".mst", L".lnk", L".bat", L".cmd", L".url", L".library-ms", L".search-ms",
        L".searchconnector-ms", L".appref-ms", L".settingcontent-ms",
    };
    for (int i = 0; i < COUNTOF(B); i++) if (_wcsicmp(ext, B[i]) == 0) return TRUE;
    return FALSE;
}

int cat_from_perceived(const wchar_t *p)
{
    if (!p || !p[0]) return -1;
    if (!_wcsicmp(p, L"image"))      return CAT_IMAGES;
    if (!_wcsicmp(p, L"video"))      return CAT_VIDEO;
    if (!_wcsicmp(p, L"audio"))      return CAT_AUDIO;
    if (!_wcsicmp(p, L"text"))       return CAT_TEXT;
    if (!_wcsicmp(p, L"document"))   return CAT_DOCS;
    if (!_wcsicmp(p, L"compressed")) return CAT_ARCHIVES;
    return -1;
}
