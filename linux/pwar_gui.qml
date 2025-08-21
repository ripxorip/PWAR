import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls.Material 2.15

ApplicationWindow {
    visible: true
    width: 780
    height: 900
    minimumWidth: 780
    minimumHeight: 900
    title: "PWAR"

    // Global theme
    Material.theme: Material.Dark
    Material.accent: "#FF6A00"     // orange accent
    Material.foreground: "#F5F5F5"
    Material.background: "#1E1E1E"

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 16

        Label {
            text: "PWAR Control Panel"
            font.pixelSize: 24
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            Layout.alignment: Qt.AlignHCenter
            Layout.fillWidth: true
        }

        // ==== Audio ====
        GroupBox {
            title: "Audio"
            Layout.fillWidth: true

            GridLayout {
                anchors.fill: parent
                anchors.margins: 14
                columns: 2
                columnSpacing: 14
                rowSpacing: 10

                Label { text: "Input" }
                ComboBox {
                    id: inputCombo
                    Layout.fillWidth: true
                    model: ["Input 1", "Input 2", "Input 3"]
                }

                Label { text: "Output" }
                ComboBox {
                    id: outputCombo
                    Layout.fillWidth: true
                    model: ["Output 1", "Output 2", "Output 3"]
                }

                Label { text: "Buffer" }
                ComboBox {
                    id: bufferSizeCombo
                    Layout.fillWidth: true
                    model: ["128", "256", "512", "1024"]
                    currentIndex: 1
                }

                Label { text: "Oneshot Mode" }
                CheckBox { id: oneshotCheck }

                // bottom breathing room
                Item { Layout.columnSpan: 2; height: 4 }
            }
        }

        // ==== Network ====
        GroupBox {
            title: "Network"
            Layout.fillWidth: true

            GridLayout {
                anchors.fill: parent
                anchors.margins: 14
                columns: 2
                columnSpacing: 14
                rowSpacing: 10

                Label { text: "IP Address" }
                TextField {
                    id: ipField
                    Layout.fillWidth: true
                    placeholderText: "e.g. 192.168.1.10"
                    inputMethodHints: Qt.ImhPreferNumbers
                }

                Label { text: "Port" }
                TextField {
                    id: portField
                    Layout.fillWidth: true
                    placeholderText: "e.g. 5600"
                    inputMethodHints: Qt.ImhDigitsOnly
                    validator: IntValidator { bottom: 1; top: 65535 }
                }

                Item { Layout.columnSpan: 2; height: 4 }
            }
        }

        // ==== Status (read-only) ====
        GroupBox {
            title: "Status"
            Layout.fillWidth: true

            GridLayout {
                anchors.fill: parent
                anchors.margins: 14
                columns: 2
                columnSpacing: 14
                rowSpacing: 10

                Label { text: "State" }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    Rectangle {
                        width: 10; height: 10; radius: 5
                        color: pwarController.running ? Material.accent : "#666666"
                        border.color: "#222222"
                    }
                    Label { text: pwarController.running ? "Running" : "Stopped" }
                }

                Label { text: "Latency (ms)" }
                Label { text: Number(pwarController.latencyMs || 0).toFixed(1) }

                Label { text: "Jitter (ms, p-p)" }
                Label { text: Number(pwarController.jitterMs || 0).toFixed(1) }

                Label { text: "RTT (ms)" }
                Label { text: Number(pwarController.rttMs || 0).toFixed(1) }

                Label { text: "Buffer (frames)" }
                Label { text: String(pwarController.bufferFrames || bufferSizeCombo.currentText) }

                Label { text: "XRuns" }
                Label { text: String(pwarController.xruns || 0) }

                Item { Layout.columnSpan: 2; height: 4 }
            }
        }

        // ==== Actions ====
        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: 12

            Button {
                text: "Start"
                onClicked: pwarController.start(
                    inputCombo.currentText,
                    outputCombo.currentText,
                    bufferSizeCombo.currentText,
                    ipField.text,
                    portField.text,
                    oneshotCheck.checked
                )
            }
            Button {
                text: "Stop"
                onClicked: pwarController.stop()
            }
        }
    }
}
