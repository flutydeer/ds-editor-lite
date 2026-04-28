import QtQuick

Item {
    id: root

    readonly property color textPrimary: "#B6B7BA"
    readonly property color textSecondary: Qt.rgba(182/255, 183/255, 186/255, 140/255)
    readonly property color cardTextColor: "#C8C9CC"
    readonly property color borderColor: "#1D1F26"
    readonly property color cardBackground: "#272A33"
    readonly property color buttonBackground: Qt.rgba(255, 255, 255, 0.06)
    readonly property color buttonHoverBackground: Qt.rgba(255, 255, 255, 0.09)

    Item {
        id: placeholder
        anchors.fill: parent
        visible: !viewModel.hasPackage

        Text {
            anchors.centerIn: parent
            text: qsTr("Select a package to view details")
            color: root.textSecondary
            font.pixelSize: 13
        }
    }

    Item {
        id: detailsView
        anchors.fill: parent
        visible: viewModel.hasPackage

        Item {
            id: header
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: headerColumn.height + 32

            Column {
                id: headerColumn
                anchors.left: parent.left
                anchors.right: buttonRow.left
                anchors.top: parent.top
                anchors.margins: 16
                spacing: 8

                Text {
                    text: viewModel.packageId
                    font.pixelSize: 24
                    color: root.textPrimary
                }

                Row {
                    spacing: 16

                    Text {
                        text: viewModel.vendor
                        font.pixelSize: 13
                        color: root.textSecondary
                        visible: text.length > 0
                    }
                    Text {
                        text: viewModel.version
                        font.pixelSize: 13
                        color: root.textSecondary
                        visible: text.length > 0
                    }
                    Text {
                        text: viewModel.copyright
                        font.pixelSize: 13
                        color: root.textSecondary
                        visible: text.length > 0
                    }
                }
            }

            Row {
                id: buttonRow
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 16
                spacing: 8

                Rectangle {
                    width: openWebsiteText.implicitWidth + 16
                    height: openWebsiteText.implicitHeight + 12
                    radius: 4
                    color: openWebsiteMa.containsMouse ? root.buttonHoverBackground : root.buttonBackground
                    visible: viewModel.websiteUrl.length > 0

                    Text {
                        id: openWebsiteText
                        anchors.centerIn: parent
                        text: qsTr("Open Website...")
                        color: root.textPrimary
                        font.pixelSize: 13
                    }
                    MouseArea {
                        id: openWebsiteMa
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: viewModel.openWebsite()
                    }
                }

                Rectangle {
                    width: verifyText.implicitWidth + 16
                    height: verifyText.implicitHeight + 12
                    radius: 4
                    color: verifyMa.containsMouse ? root.buttonHoverBackground : root.buttonBackground

                    Text {
                        id: verifyText
                        anchors.centerIn: parent
                        text: qsTr("Verify")
                        color: root.textPrimary
                        font.pixelSize: 13
                    }
                    MouseArea {
                        id: verifyMa
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: viewModel.verify()
                    }
                }

                Rectangle {
                    width: uninstallText.implicitWidth + 16
                    height: uninstallText.implicitHeight + 12
                    radius: 4
                    color: uninstallMa.containsMouse ? root.buttonHoverBackground : root.buttonBackground

                    Text {
                        id: uninstallText
                        anchors.centerIn: parent
                        text: qsTr("Uninstall")
                        color: root.textPrimary
                        font.pixelSize: 13
                    }
                    MouseArea {
                        id: uninstallMa
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: viewModel.uninstall()
                    }
                }
            }

            Rectangle {
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                height: 1
                color: root.borderColor
            }
        }

        Flickable {
            anchors.top: header.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            contentWidth: width
            contentHeight: contentColumn.height
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            flickableDirection: Flickable.VerticalFlick

            Column {
                id: contentColumn
                width: parent.width
                topPadding: 12
                leftPadding: 12
                rightPadding: 12
                spacing: 12

                Column {
                    width: parent.width - 24
                    spacing: 6

                    Text {
                        text: qsTr("Description")
                        color: root.textPrimary
                        font.pixelSize: 13
                        leftPadding: 10
                    }

                    Rectangle {
                        width: parent.width
                        height: descriptionText.implicitHeight + 32
                        color: root.cardBackground
                        radius: 8

                        Text {
                            id: descriptionText
                            anchors.fill: parent
                            anchors.margins: 16
                            text: viewModel.description.length > 0
                                  ? viewModel.description
                                  : qsTr("No description.")
                            color: root.cardTextColor
                            wrapMode: Text.Wrap
                            font.pixelSize: 13
                        }
                    }
                }

                Column {
                    width: parent.width - 24
                    spacing: 6

                    Text {
                        text: qsTr("ReadMe")
                        color: root.textPrimary
                        font.pixelSize: 13
                        leftPadding: 10
                    }

                    Rectangle {
                        width: parent.width
                        height: readMeText.implicitHeight + 32
                        color: root.cardBackground
                        radius: 8

                        Text {
                            id: readMeText
                            anchors.fill: parent
                            anchors.margins: 16
                            text: viewModel.readMeContent.length > 0
                                  ? viewModel.readMeContent
                                  : qsTr("No readme file.")
                            color: root.cardTextColor
                            wrapMode: Text.Wrap
                            font.pixelSize: 13
                        }
                    }
                }
            }
        }
    }
}
