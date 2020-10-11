# SlackLogViewer
This is an application to view json files exported from your Slack workspace with Slack-like GUI. You can browse any messages that cannot be shown in your workspace because of the limit of the free plan. You can also seach for them by some words or regular expression and download files if they are not hidden by the storage limit.

<img src="https://user-images.githubusercontent.com/53743073/95690360-18ed7980-0c52-11eb-92fc-0d92b7fa4eab.png" width="640px">

### Usage
This is currently only available for Windows 64bit.

1. Export a zip file from your Slack workspace and unzip it.
1. Download prebuilt binary from [Releases](https://github.com/thayakawa-gh/SlackLogViewer/releases) and unzip into any folder. No installation is required.
1. Execute the SlackLogViewer.exe.
1. Click "Open" from the menu button in the top left corner of the window, and select the folder of exported json files.

### Acknowledgements
This application depends on Qt 5.15.1 for GUI and emojicpp developed by Shalitha Suranga to convert icons into unicode.
