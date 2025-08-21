import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls.Material 2.15
import QtGraphicalEffects 1.15

ApplicationWindow {
    visible: true
    width: 780
    height: 900
    minimumWidth: 780
    minimumHeight: 900
    title: "PWAR"

    // Enhanced graphite gray theme
    Material.theme: Material.Dark
    Material.accent: "#FF6A00"     // vibrant orange accent
    Material.foreground: "#E0E0E0"
    Material.background: "#2B2B2B" // graphite gray background
    Material.primary: "#FF6A00"
    
    // Custom color properties
    property color graphiteDark: "#1A1A1A"
    property color graphiteMedium: "#2B2B2B"
    property color graphiteLight: "#3A3A3A"
    property color orangeAccent: "#FF6A00"
    property color orangeHover: "#FF8533"
    property color textPrimary: "#E0E0E0"
    property color textSecondary: "#B0B0B0"
    
    // Background with subtle gradient
    background: Rectangle {
        color: graphiteDark
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#1E1E1E" }
            GradientStop { position: 1.0; color: "#2B2B2B" }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 16

        Label {
            text: "PWAR Control Panel"
            font.pixelSize: 28
            font.bold: true
            font.family: "Segoe UI, Arial, sans-serif"
            horizontalAlignment: Text.AlignHCenter
            Layout.alignment: Qt.AlignHCenter
            Layout.fillWidth: true
            color: orangeHover
            
            // Add subtle shadow effect
            layer.enabled: true
            layer.effect: DropShadow {
                horizontalOffset: 0
                verticalOffset: 2
                radius: 4
                color: "#40000000"
            }
        }

        // ==== Audio ====
        GroupBox {
            title: "Audio"
            Layout.fillWidth: true
            
            background: Rectangle {
                color: graphiteLight
                radius: 8
                border.color: orangeAccent
                border.width: 1
                
                // Subtle inner shadow
                Rectangle {
                    anchors.fill: parent
                    anchors.margins: 1
                    color: "transparent"
                    radius: 7
                    border.color: "#40FFFFFF"
                    border.width: 1
                }
            }
            
            label: Label {
                text: parent.title
                color: orangeAccent
                font.bold: true
                font.pixelSize: 14
                leftPadding: 8
                rightPadding: 8
                background: Rectangle {
                    color: graphiteDark
                    radius: 4
                }
            }

            GridLayout {
                anchors.fill: parent
                anchors.margins: 14
                columns: 2
                columnSpacing: 14
                rowSpacing: 10

                Label { 
                    text: "Input"
                    color: textPrimary
                    font.bold: true
                }
                ComboBox {
                    id: inputCombo
                    Layout.fillWidth: true
                    model: ["Input 1", "Input 2", "Input 3"]
                }

                Label { 
                    text: "Output"
                    color: textPrimary
                    font.bold: true
                }
                ComboBox {
                    id: outputCombo
                    Layout.fillWidth: true
                    model: ["Output 1", "Output 2", "Output 3"]
                }

                Label { 
                    text: "Buffer"
                    color: textPrimary
                    font.bold: true
                }
                ComboBox {
                    id: bufferSizeCombo
                    Layout.fillWidth: true
                    model: ["128", "256", "512", "1024"]
                    currentIndex: 1
                }

                Label { 
                    text: "Oneshot Mode"
                    color: textPrimary
                    font.bold: true
                }
                CheckBox { id: oneshotCheck }

                // bottom breathing room
                Item { Layout.columnSpan: 2; height: 4 }
            }
        }

        // ==== Network ====
        GroupBox {
            title: "Network"
            Layout.fillWidth: true
            
            background: Rectangle {
                color: graphiteLight
                radius: 8
                border.color: orangeAccent
                border.width: 1
                
                Rectangle {
                    anchors.fill: parent
                    anchors.margins: 1
                    color: "transparent"
                    radius: 7
                    border.color: "#40FFFFFF"
                    border.width: 1
                }
            }
            
            label: Label {
                text: parent.title
                color: orangeAccent
                font.bold: true
                font.pixelSize: 14
                leftPadding: 8
                rightPadding: 8
                background: Rectangle {
                    color: graphiteDark
                    radius: 4
                }
            }

            GridLayout {
                anchors.fill: parent
                anchors.margins: 14
                columns: 2
                columnSpacing: 14
                rowSpacing: 10

                Label { 
                    text: "IP Address"
                    color: textPrimary
                    font.bold: true
                }
                TextField {
                    id: ipField
                    Layout.fillWidth: true
                    placeholderText: "e.g. 192.168.1.10"
                    inputMethodHints: Qt.ImhPreferNumbers
                }

                Label { 
                    text: "Port"
                    color: textPrimary
                    font.bold: true
                }
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
            
            background: Rectangle {
                color: graphiteLight
                radius: 8
                border.color: orangeAccent
                border.width: 1
                
                Rectangle {
                    anchors.fill: parent
                    anchors.margins: 1
                    color: "transparent"
                    radius: 7
                    border.color: "#40FFFFFF"
                    border.width: 1
                }
            }
            
            label: Label {
                text: parent.title
                color: orangeAccent
                font.bold: true
                font.pixelSize: 14
                leftPadding: 8
                rightPadding: 8
                background: Rectangle {
                    color: graphiteDark
                    radius: 4
                }
            }

            GridLayout {
                anchors.fill: parent
                anchors.margins: 14
                columns: 2
                columnSpacing: 14
                rowSpacing: 10

                Label { 
                    text: "State"
                    color: textPrimary
                    font.bold: true
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Rectangle {
                        width: 16; height: 16; radius: 8
                        color: pwarController.running ? orangeAccent : "#666666"
                        border.color: pwarController.running ? "#FFFFFF" : "#444444"
                        border.width: 2
                        
                        // Add pulsing animation when running
                        SequentialAnimation on opacity {
                            running: pwarController.running
                            loops: Animation.Infinite
                            NumberAnimation { to: 0.6; duration: 1000 }
                            NumberAnimation { to: 1.0; duration: 1000 }
                        }
                        
                        // Inner glow effect
                        Rectangle {
                            anchors.centerIn: parent
                            width: 8; height: 8; radius: 4
                            color: pwarController.running ? "#FFFFFF" : "transparent"
                            opacity: 0.8
                        }
                    }
                    Label { 
                        text: pwarController.running ? "Running" : "Stopped"
                        color: pwarController.running ? orangeAccent : textSecondary
                        font.bold: pwarController.running
                    }
                }

                Label { 
                    text: "Latency (ms)"
                    color: textPrimary
                    font.bold: true
                }
                Label { 
                    text: Number(pwarController.latencyMs || 0).toFixed(1)
                    color: orangeAccent
                    font.bold: true
                }

                Label { 
                    text: "Jitter (ms, p-p)"
                    color: textPrimary
                    font.bold: true
                }
                Label { 
                    text: Number(pwarController.jitterMs || 0).toFixed(1)
                    color: orangeAccent
                    font.bold: true
                }

                Label { 
                    text: "RTT (ms)"
                    color: textPrimary
                    font.bold: true
                }
                Label { 
                    text: Number(pwarController.rttMs || 0).toFixed(1)
                    color: orangeAccent
                    font.bold: true
                }

                Label { 
                    text: "Buffer (frames)"
                    color: textPrimary
                    font.bold: true
                }
                Label { 
                    text: String(pwarController.bufferFrames || bufferSizeCombo.currentText)
                    color: orangeAccent
                    font.bold: true
                }

                Label { 
                    text: "XRuns"
                    color: textPrimary
                    font.bold: true
                }
                Label { 
                    text: String(pwarController.xruns || 0)
                    color: orangeAccent
                    font.bold: true
                }

                Item { Layout.columnSpan: 2; height: 4 }
            }
        }

        // ==== Actions ====
        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: 16

            Button {
                text: "Start"
                font.bold: true
                font.pixelSize: 14
                
                background: Rectangle {
                    implicitWidth: 100
                    implicitHeight: 40
                    color: parent.pressed ? "#E55A00" : (parent.hovered ? orangeHover : orangeAccent)
                    radius: 6
                    border.color: parent.pressed ? "#CC5500" : orangeAccent
                    border.width: 2
                    
                    // Add gradient effect
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: parent.pressed ? "#E55A00" : (parent.parent.hovered ? "#FF8533" : "#FF6A00") }
                        GradientStop { position: 1.0; color: parent.pressed ? "#CC5500" : (parent.parent.hovered ? "#E55A00" : "#E55A00") }
                    }
                    
                    // Shadow effect
                    Rectangle {
                        anchors.fill: parent
                        anchors.topMargin: parent.parent.pressed ? 0 : 2
                        radius: parent.radius
                        color: "#40000000"
                        z: -1
                    }
                    
                    Behavior on color { ColorAnimation { duration: 150 } }
                }
                
                contentItem: Text {
                    text: parent.text
                    font: parent.font
                    color: "#FFFFFF"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                
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
                font.bold: true
                font.pixelSize: 14
                
                background: Rectangle {
                    implicitWidth: 100
                    implicitHeight: 40
                    color: parent.pressed ? "#4A4A4A" : (parent.hovered ? "#5A5A5A" : graphiteLight)
                    radius: 6
                    border.color: parent.pressed ? textSecondary : (parent.hovered ? orangeAccent : textSecondary)
                    border.width: 2
                    
                    // Shadow effect
                    Rectangle {
                        anchors.fill: parent
                        anchors.topMargin: parent.parent.pressed ? 0 : 2
                        radius: parent.radius
                        color: "#40000000"
                        z: -1
                    }
                    
                    Behavior on color { ColorAnimation { duration: 150 } }
                    Behavior on border.color { ColorAnimation { duration: 150 } }
                }
                
                contentItem: Text {
                    text: parent.text
                    font: parent.font
                    color: parent.hovered ? orangeAccent : textPrimary
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    
                    Behavior on color { ColorAnimation { duration: 150 } }
                }
                
                onClicked: pwarController.stop()
            }
        }
    }
}
