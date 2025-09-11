import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls.Material 2.15
import QtGraphicalEffects 1.15

ApplicationWindow {
    visible: true
    width: 780
    height: 1050
    minimumWidth: 780
    minimumHeight: 1050
    title: "PWAR Control Panel"
    
    // Set window icon for Wayland compatibility
    property var windowIcon: appIcon
    
    Component.onCompleted: {
        // Try to set the window icon on Wayland
        if (typeof appIcon !== 'undefined') {
            console.log("Setting window icon from resource")
        }
    }

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
    
    // Layout properties
    property int statusValueLeftMargin: 450
    
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

        // ==== Logo and Title Section ====
        ColumnLayout {
            Layout.alignment: Qt.AlignHCenter
            Layout.fillWidth: true
            spacing: 12

            Image {
                source: "qrc:/logo_round.png"
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: 160
                Layout.preferredHeight: 160
                fillMode: Image.PreserveAspectFit
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
                    model: pwarController.inputPorts
                    currentIndex: {
                        var idx = model.indexOf(pwarController.selectedInputPort);
                        return idx >= 0 ? idx : -1;
                    }
                    onCurrentTextChanged: {
                        if (currentText !== pwarController.selectedInputPort) {
                            pwarController.selectedInputPort = currentText;
                        }
                    }
                }

                Label { 
                    text: "Output Left"
                    color: textPrimary
                    font.bold: true
                }
                ComboBox {
                    id: outputLeftCombo
                    Layout.fillWidth: true
                    model: pwarController.outputPorts
                    currentIndex: {
                        var idx = model.indexOf(pwarController.selectedOutputLeftPort);
                        return idx >= 0 ? idx : -1;
                    }
                    onCurrentTextChanged: {
                        if (currentText !== pwarController.selectedOutputLeftPort) {
                            pwarController.selectedOutputLeftPort = currentText;
                        }
                    }
                }

                Label { 
                    text: "Output Right"
                    color: textPrimary
                    font.bold: true
                }
                ComboBox {
                    id: outputRightCombo
                    Layout.fillWidth: true
                    model: pwarController.outputPorts
                    currentIndex: {
                        var idx = model.indexOf(pwarController.selectedOutputRightPort);
                        return idx >= 0 ? idx : -1;
                    }
                    onCurrentTextChanged: {
                        if (currentText !== pwarController.selectedOutputRightPort) {
                            pwarController.selectedOutputRightPort = currentText;
                        }
                    }
                }

                Label { 
                    text: "Chunk Size"
                    color: textPrimary
                    font.bold: true
                }
                ComboBox {
                    id: bufferSizeCombo
                    Layout.fillWidth: true
                    model: [32, 64, 128]
                    currentIndex: {
                        var idx = model.indexOf(pwarController.bufferSize);
                        return idx >= 0 ? idx : 0;
                    }
                    onCurrentValueChanged: {
                        if (currentValue !== pwarController.bufferSize) {
                            pwarController.bufferSize = currentValue;
                        }
                    }
                }

                Label { 
                    text: "Ring Buffer Depth"
                    color: textPrimary
                    font.bold: true
                }
                ComboBox {
                    id: ringBufferDepthCombo
                    Layout.fillWidth: true
                    model: [32, 64, 128, 256, 512, 1024, 2048, 4096, 8192]
                    currentIndex: {
                        var idx = model.indexOf(pwarController.ringBufferDepth);
                        return idx >= 0 ? idx : 2; // default to 2048
                    }
                    onCurrentValueChanged: {
                        if (currentValue !== pwarController.ringBufferDepth) {
                            pwarController.ringBufferDepth = currentValue;
                        }
                    }
                }

                Label { 
                    text: "Passthrough Test"
                    color: textPrimary
                    font.bold: true
                }
                CheckBox { 
                    id: passthroughCheck
                    checked: pwarController.passthroughTest
                    onCheckedChanged: pwarController.passthroughTest = checked
                }

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
                    color: textPrimary
                    placeholderTextColor: textSecondary
                    selectByMouse: true
                    text: pwarController.streamIp
                    onTextChanged: {
                        if (text !== pwarController.streamIp) {
                            pwarController.streamIp = text;
                        }
                    }
                    
                    background: Rectangle {
                        color: graphiteMedium
                        radius: 4
                        border.color: parent.activeFocus ? orangeAccent : (parent.hovered ? orangeHover : "#555555")
                        border.width: parent.activeFocus ? 2 : 1
                        
                        Behavior on border.color { ColorAnimation { duration: 150 } }
                        Behavior on border.width { NumberAnimation { duration: 150 } }
                        
                        // Subtle glow when focused
                        Rectangle {
                            anchors.fill: parent
                            anchors.margins: -2
                            radius: parent.radius + 2
                            color: "transparent"
                            border.color: orangeAccent
                            border.width: parent.parent.activeFocus ? 1 : 0
                            opacity: parent.parent.activeFocus ? 0.3 : 0
                            
                            Behavior on opacity { NumberAnimation { duration: 150 } }
                            Behavior on border.width { NumberAnimation { duration: 150 } }
                        }
                    }
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
                    color: textPrimary
                    placeholderTextColor: textSecondary
                    selectByMouse: true
                    text: pwarController.streamPort.toString()
                    onTextChanged: {
                        var portNumber = parseInt(text);
                        if (!isNaN(portNumber) && portNumber !== pwarController.streamPort) {
                            pwarController.streamPort = portNumber;
                        }
                    }
                    
                    background: Rectangle {
                        color: graphiteMedium
                        radius: 4
                        border.color: parent.activeFocus ? orangeAccent : (parent.hovered ? orangeHover : "#555555")
                        border.width: parent.activeFocus ? 2 : 1
                        
                        Behavior on border.color { ColorAnimation { duration: 150 } }
                        Behavior on border.width { NumberAnimation { duration: 150 } }
                        
                        // Subtle glow when focused
                        Rectangle {
                            anchors.fill: parent
                            anchors.margins: -2
                            radius: parent.radius + 2
                            color: "transparent"
                            border.color: orangeAccent
                            border.width: parent.parent.activeFocus ? 1 : 0
                            opacity: parent.parent.activeFocus ? 0.3 : 0
                            
                            Behavior on opacity { NumberAnimation { duration: 150 } }
                            Behavior on border.width { NumberAnimation { duration: 150 } }
                        }
                    }
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
                    Layout.leftMargin: statusValueLeftMargin
                    spacing: 12
                    Rectangle {
                        width: 16; height: 16; radius: 8
                        color: pwarController.isRunning ? orangeAccent : "#666666"
                        border.color: pwarController.isRunning ? "#FFFFFF" : "#444444"
                        border.width: 2
                        
                        // Add pulsing animation when running
                        SequentialAnimation on opacity {
                            running: pwarController.isRunning
                            loops: Animation.Infinite
                            NumberAnimation { to: 0.6; duration: 1000 }
                            NumberAnimation { to: 1.0; duration: 1000 }
                        }
                        
                        // Inner glow effect
                        Rectangle {
                            anchors.centerIn: parent
                            width: 8; height: 8; radius: 4
                            color: pwarController.isRunning ? "#FFFFFF" : "transparent"
                            opacity: 0.8
                        }
                    }
                    Label { 
                        text: pwarController.isRunning ? "Running" : "Stopped"
                        color: pwarController.isRunning ? orangeAccent : textSecondary
                        font.bold: pwarController.isRunning
                    }
                }

                Label { 
                    text: "Ring Buffer Avg (ms)"
                    color: textPrimary
                    font.bold: true
                }
                Label { 
                    text: Number(pwarController.ringBufferAvgMs || 0).toFixed(3)
                    color: orangeAccent
                    font.bold: true
                    Layout.fillWidth: true
                    Layout.leftMargin: statusValueLeftMargin
                }

                Label { 
                    text: "RTT (ms)"
                    color: textPrimary
                    font.bold: true
                }
                Label { 
                    text: Number(pwarController.rttMinMs || 0).toFixed(3) + "/" + 
                          Number(pwarController.rttMaxMs || 0).toFixed(3) + "/" + 
                          Number(pwarController.rttAvgMs || 0).toFixed(3)
                    color: orangeAccent
                    font.bold: true
                    Layout.fillWidth: true
                    Layout.leftMargin: statusValueLeftMargin
                }

                Label { 
                    text: "XRUNS"
                    color: textPrimary
                    font.bold: true
                }
                Label { 
                    text: String(pwarController.xruns || 0)
                    color: orangeAccent
                    font.bold: true
                    Layout.fillWidth: true
                    Layout.leftMargin: statusValueLeftMargin
                }

                Label { 
                    text: "Chunk Size"
                    color: textPrimary
                    font.bold: true
                }
                Label { 
                    text: String(pwarController.currentWindowsBufferSize || 0)
                    color: orangeAccent
                    font.bold: true
                    Layout.fillWidth: true
                    Layout.leftMargin: statusValueLeftMargin
                }

                Label { 
                    text: "Status"
                    color: textPrimary
                    font.bold: true
                }
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: statusLabel.height
                    Layout.leftMargin: statusValueLeftMargin
                    color: "transparent"
                    
                    Label { 
                        id: statusLabel
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        text: pwarController.status
                        color: orangeAccent
                        font.bold: true
                        width: parent.width
                        wrapMode: Text.WordWrap
                        elide: Text.ElideRight
                    }
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
                enabled: !pwarController.isRunning
                
                background: Rectangle {
                    implicitWidth: 100
                    implicitHeight: 40
                    color: parent.enabled ? (parent.pressed ? "#E55A00" : (parent.hovered ? orangeHover : orangeAccent)) : "#666666"
                    radius: 6
                    border.color: parent.enabled ? (parent.pressed ? "#CC5500" : orangeAccent) : "#444444"
                    border.width: 2
                    
                    // Add gradient effect
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: parent.enabled ? (parent.pressed ? "#E55A00" : (parent.hovered ? "#FF8533" : "#FF6A00")) : "#666666" }
                        GradientStop { position: 1.0; color: parent.enabled ? (parent.pressed ? "#CC5500" : (parent.hovered ? "#E55A00" : "#E55A00")) : "#444444" }
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
                    color: parent.enabled ? "#FFFFFF" : "#AAAAAA"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                
                onClicked: pwarController.start()
            }
            
            Button {
                text: "Stop"
                font.bold: true
                font.pixelSize: 14
                enabled: pwarController.isRunning
                
                background: Rectangle {
                    implicitWidth: 100
                    implicitHeight: 40
                    color: parent.enabled ? (parent.pressed ? "#4A4A4A" : (parent.hovered ? "#5A5A5A" : graphiteLight)) : "#333333"
                    radius: 6
                    border.color: parent.enabled ? (parent.pressed ? textSecondary : (parent.hovered ? orangeAccent : textSecondary)) : "#444444"
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
                    color: parent.enabled ? (parent.hovered ? orangeAccent : textPrimary) : "#AAAAAA"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    
                    Behavior on color { ColorAnimation { duration: 150 } }
                }
                
                onClicked: pwarController.stop()
            }
        }
    }
}
