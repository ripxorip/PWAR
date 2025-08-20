import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

ApplicationWindow {
    visible: true
    width: 400
    height: 220
    title: "PWAR Qt QML GUI"
    color: "#232629"

    ColumnLayout {
        anchors.centerIn: parent
        spacing: 18

        Label {
            text: "PWAR QML GUI is running!"
            font.pixelSize: 28
            font.bold: true
            color: "#e5e9f0"
            horizontalAlignment: Text.AlignHCenter
            Layout.alignment: Qt.AlignHCenter
        }
        ComboBox {
            id: exampleDropdown
            model: ["Option 1", "Option 2", "Option 3", "Option 4"]
            font.pixelSize: 18
            width: 180
            Layout.alignment: Qt.AlignHCenter
            background: Rectangle {
                color: "#4c566a"
                radius: 8
            }
            contentItem: Text {
                text: exampleDropdown.currentText
                color: "#eceff4"
                font.pixelSize: 18
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
        }
        Button {
            text: "Start"
            font.pixelSize: 20
            font.bold: true
            background: Rectangle {
                color: "#5e81ac"
                radius: 8
            }
            contentItem: Text {
                text: parent.text
                color: "#eceff4"
                font.pixelSize: 20
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            width: 160
            height: 48
            Layout.alignment: Qt.AlignHCenter
        }
    }
}
