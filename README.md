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

## Build

FolderView is a native Windows C++ application built with Visual Studio/MSBuild and vcpkg.

```powershell
msbuild /m /p:Configuration=Release /p:Platform=x64 FolderView/FolderView.sln /p:VcpkgTriplet=x64-windows /p:VcpkgEnableManifest=true
```

Release builds include `FolderView.exe` and the app-local `zlib1.dll`. The Microsoft Visual C++ runtime is provided by the WinGet package dependency `Microsoft.VCRedist.2015+.x64`.

GitHub Actions release builds are created from `v*` tags. The workflow stamps `FolderView.rc` from the tag version before building, so `v1.2.3` produces `FileVersion` and `ProductVersion` `1.2.3`.

## License

MIT
