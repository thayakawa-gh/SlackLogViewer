# SlackLogViewer
This is an application to view json files exported from your Slack workspace with Slack-like GUI.  
By using this, you can:
* browse any messages that cannot be shown in your workspaces because of the limit of the free plan.
* display threads and reactions.
* search for them by some words or regular expression.
* view or download files, if they are not hidden by the storage limit.

<img src="https://user-images.githubusercontent.com/53743073/95690436-c19bd900-0c52-11eb-9889-1ca5076189ee.png" width="640px">

### Usage
This is currently only available for Windows 64bit.

1. Export a zip file from your Slack workspace and unzip it.
1. Download prebuilt binary from [Releases](https://github.com/thayakawa-gh/SlackLogViewer/releases) and unzip into any folder. No installation is required.
1. Execute the SlackLogViewer.exe.
1. Click "Open" from the menu button in the top left corner of the window, and select the folder of exported json files.

### Acknowledgements
This application depends on Qt 5.14.2 for GUI and emojicpp developed by Shalitha Suranga to convert icons into unicode.
