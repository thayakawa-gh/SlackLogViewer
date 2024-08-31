# SlackLogViewer
This is an application to view json files exported from your Slack workspace with Slack-like GUI.  
By using this, you can:
* browse any messages that cannot be shown in your workspaces because of the limit of the free plan.
* display threads and reactions.
* search for them by some words or regular expression.
* view or download files, if they are not hidden by the storage limit.

<img src="https://user-images.githubusercontent.com/53743073/95690436-c19bd900-0c52-11eb-9889-1ca5076189ee.png" width="640px">

### Building
For Windows and macOS, you can download a prebuilt binary from [Releases](https://github.com/thayakawa-gh/SlackLogViewer/releases).

If necessary, you can build SlackLogViewer with CMake as in the following commands.
```
git clone https://github.com/thayakawa-gh/SlackLogViewer
cd SlackLogViewer
mkdir build
cd build
cmake ../SlackLogViewer -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=path_to_install_dir
make
make install
```

#### Note
This application is written in C++17 and confirmed to be buildable with the following compilers.

* Visual Studio 2019/2022
* GCC 9.5.0/11.3.0
* Clang/LLVM 12.0.0

Make sure you have installed the following dependencies.

* Qt6 (6.2 or higher) with QtWebEngine
* zlib
* QuaZIP
* TBB (for GCC 11.0 or higher)

### Usage
On Windows, this application requires Visual C++ Redistributable for Visual Studio 2019.

1. Export a zip file from your Slack workspace. If you are a free plan user and want to dump direct messages or private channnels, you can also use [slackdump](https://github.com/rusq/slackdump), which can export these messages in a format compatible with SlackLogViewer. If you want SlackLogViewer to download and show attachments, it is recommended to use slackdump with `-export-token ???` option, replacing ??? with your valid token.
1. Download prebuilt binary from [Releases](https://github.com/thayakawa-gh/SlackLogViewer/releases) and unzip into any folder.
1. Execute the SlackLogViewer.
1. Click "Open" from the menu button in the top left corner of the window, and select the zip file or the folder of exported json files.

### Acknowledgments
This application depends on the following third-party libraries and tools.

* Qt Copyright (C) 2020 The Qt Company Ltd.
* emojicpp Copyright (c) 2018 Shalitha Suranga
* CMakeHelpers Copyright (C) 2015 halex2005
* QuaZip Copyright (C) 2005-2020 Sergey A. Tachenov and contributors
* zlib Copyright (C) 1995-2017 Jean-loup Gailly and Mark Adler
