# FolderView

Lightweight image viewer for folders, ZIP archives, JPEG, PNG, and WebP files.

FolderView opens an image file, folder, or archive and lets you browse images quickly with keyboard navigation.

## Install

The recommended Windows installation method is WinGet:

```powershell
winget install mbobka.FolderView
```

After installation, the portable command alias is:

```powershell
fview <path>
```

## Shortcuts

| Shortcut | Action |
| --- | --- |
| `Left`, `Numpad 8`, mouse wheel up | Previous image |
| `Right`, `Numpad 2`, mouse wheel down | Next image |
| `Home`, `Numpad 7` | First image |
| `End`, `Numpad 1` | Last image |
| `Page Up` | Previous image, or previous ZIP archive in ZIP mode |
| `Page Down` | Next image, or next ZIP archive in ZIP mode |
| `Up` | Previous ZIP archive in ZIP mode |
| `Down` | Next ZIP archive in ZIP mode |
| `+`, `Ctrl+Up` | Zoom in |
| `-`, `Ctrl+Down` | Zoom out |
| `0`, `Numpad *` | Toggle actual size / fit to window |
| `Ctrl+C`, `Ctrl+Insert` | Copy the current file to the clipboard |
| `Alt+?`, `Alt+/` | About dialog |
| `Esc` | Exit |
| Left mouse drag | Pan the image |

## Build with Docker

FolderView is written in freestanding C and cross-compiled for windows-amd64 inside a **Linux** Docker container. The toolchain is `clang-cl` + `lld-link` + `llvm-rc`; Windows SDK headers and import libraries are fetched with [xwin](https://github.com/Jake-Shadle/xwin). The host machine only needs the source tree and Docker with BuildKit support.

```powershell
.\build\Invoke-DockerBuild.ps1
```

The output is written to `out-win-amd64\` and includes `FolderView.exe`, license notices, a portable ZIP, and a SHA256 file. The required `inflate` sources from zlib are compiled in `Z_SOLO` mode with WinAPI heap callbacks, so no `zlib1.dll` is shipped. The portable ZIP includes the FolderView license and the zlib notice. The first build downloads the Windows SDK and toolchain, so it takes longer than later cached builds.

To stamp a release version inside the container:

```powershell
.\build\Invoke-DockerBuild.ps1 -Version 1.2.3
```

The executable has its own entry point and is linked with `/NODEFAULTLIB`. It does not use MSVCP, VCRUNTIME, or UCRT; its runtime imports are Windows system DLLs only.

GitHub Actions release builds are created from `v*` tags and run on a Linux runner. The Docker build stamps `FolderView.rc` from the tag version inside the container, so `v1.2.3` produces `FileVersion` and `ProductVersion` `1.2.3`.

## License

FolderView is distributed under the [MIT License](LICENSE). See
[Third-Party Software Notices](THIRD-PARTY-NOTICES.txt) for the zlib license.
