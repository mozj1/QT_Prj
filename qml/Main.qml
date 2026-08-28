import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

Rectangle {
    id: root
    width: 1180
    height: 720
    color: "#FFFFFF"

    property string appFontFamily: "宋体"
    property int appNormalFontWeight: Font.Light
    property int appEmphasisFontWeight: Font.Medium
    property bool contentOnly: false
    property int activePage: uiController.activePage
    property int lastParameterMatchedRow: -1
    property int lastMonitorMatchedRow: -1
    property var parameterColumnTitles: ["选择", "寄存器地址", "功能说明", "参数值", "默认值", "单位", "最小值", "最大值", "属性"]
    property var parameterColumnWidths: [56, 96, 220, 150, 110, 120, 100, 120, 70]
    property var parameterColumnEffectiveWidths: [56, 96, 220, 150, 110, 120, 100, 120, 70]
    property var parameterColumnMinWidths: [42, 64, 110, 82, 64, 64, 64, 72, 48]
    property real parameterViewportWidth: 0
    property var monitorColumnTitles: ["选择", "寄存器地址", "监控名称", "当前值", "单位", "备注"]
    property var monitorColumnRatios: [0.08, 0.14, 0.32, 0.14, 0.12, 0.20]
    property var faultColumnTitles: ["寄存器地址", "Bit位", "故障说明", "当前值", "备注"]
    property var faultColumnRatios: [0.16, 0.12, 0.34, 0.14, 0.24]
    property bool runWindowVisible: false
    property bool runWindowEmbedded: false
    property string runWindowMode: "position"
    property string runWindowTitle: "定位运行"
    property string runDockEdge: "right"
    property string runDockPreviewEdge: ""
    property string effectiveRunDockEdge: runDockPreviewEdge.length > 0 ? runDockPreviewEdge : runDockEdge
    property bool runDockSlotVisible: runWindowVisible && (runWindowEmbedded || runDockPreviewEdge.length > 0)
    property real runDockFraction: 0.5
    property real runFloatX: 340
    property real runFloatY: 120
    property real runFloatWidth: 492
    property real runFloatHeight: 252

    component AppButton: Button {
        id: control
        implicitHeight: 30
        leftPadding: 10
        rightPadding: 10
        topPadding: 4
        bottomPadding: 4
        contentItem: Text {
            text: control.text
            color: "#000000"
            font.family: root.appFontFamily
            font.weight: root.appNormalFontWeight
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
        background: Rectangle {
            color: control.down ? "#D4E8C5" : (control.highlighted || control.checked ? "#D8EFC8" : (control.hovered ? "#E5F2D8" : "#FFFFFF"))
            border.color: control.hovered || control.highlighted ? "#7DAAA6" : "#9E9E9E"
            border.width: 1
        }
    }

    component AppCheckBox: CheckBox {
        id: control
        implicitWidth: 18
        implicitHeight: 18
        spacing: 0
        indicator: Rectangle {
            implicitWidth: 16
            implicitHeight: 16
            x: 1
            y: 1
            color: "#FFFFFF"
            border.color: control.hovered ? "#7DAAA6" : "#777777"
            border.width: 1
            Rectangle {
                anchors.centerIn: parent
                width: 9
                height: 9
                visible: control.checked
                color: "#0070C0"
            }
        }
        contentItem: Item { }
    }

    component AppComboBox: ComboBox {
        id: control
        property color fieldColor: "#FFFFFF"
        property string displayTextOverride: ""
        signal enterPressed(string text)
        signal editingFinishedText(string text)
        implicitHeight: 28
        leftPadding: 8
        rightPadding: 24
        editable: false
        function resolvedText() {
            if (displayTextOverride.length > 0) {
                return displayTextOverride
            }
            if (editable && editText.length > 0) {
                return editText
            }
            return displayText
        }
        onDisplayTextOverrideChanged: {
            if (editable && contentItem && !contentItem.activeFocus) {
                editText = displayTextOverride
            }
        }
        contentItem: TextInput {
            text: control.editable && activeFocus ? control.editText : control.resolvedText()
            readOnly: !control.editable
            enabled: true
            selectByMouse: control.editable
            color: "#000000"
            font.family: root.appFontFamily
            font.weight: root.appNormalFontWeight
            horizontalAlignment: TextInput.AlignHCenter
            verticalAlignment: TextInput.AlignVCenter
            clip: true
            onTextEdited: {
                if (control.editable) {
                    control.editText = text
                }
            }
            Keys.onReturnPressed: control.enterPressed(text)
            onActiveFocusChanged: {
                if (activeFocus && control.editable && control.displayTextOverride.length > 0) {
                    control.editText = control.displayTextOverride
                    return
                }
                if (!activeFocus && control.editable) {
                    control.editingFinishedText(control.editText)
                }
            }
        }
        indicator: Canvas {
            x: control.width - width - 8
            y: (control.height - height) / 2
            width: 10
            height: 6
            contextType: "2d"
            onPaint: {
                context.reset()
                context.moveTo(0, 0)
                context.lineTo(width, 0)
                context.lineTo(width / 2, height)
                context.closePath()
                context.fillStyle = "#000000"
                context.fill()
            }
        }
        background: Rectangle {
            color: control.fieldColor
            border.color: control.activeFocus ? "#5A8FD8" : "#9E9E9E"
            border.width: 1
        }
        delegate: ItemDelegate {
            width: control.width
            height: 28
            highlighted: control.highlightedIndex === index
            background: Rectangle {
                color: highlighted ? "#DDEAF7" : "#FFFFFF"
            }
            contentItem: Text {
                text: modelData
                color: "#000000"
                font.family: root.appFontFamily
                font.weight: root.appNormalFontWeight
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }
        }
        popup: Popup {
            y: control.height
            width: control.width
            implicitHeight: Math.min(contentItem.implicitHeight, 220)
            padding: 1
            background: Rectangle {
                color: "#FFFFFF"
                border.color: "#9E9E9E"
            }
            contentItem: ListView {
                clip: true
                implicitHeight: contentHeight
                model: control.popup.visible ? control.delegateModel : null
                currentIndex: control.highlightedIndex
                ScrollBar.vertical: ScrollBar { }
            }
        }
    }

    component AppSpinBox: SpinBox {
        id: control
        implicitHeight: 28
        editable: true
        contentItem: TextInput {
            z: 2
            text: control.textFromValue(control.value, control.locale)
            color: "#000000"
            font.family: root.appFontFamily
            font.weight: root.appNormalFontWeight
            selectionColor: "#DDEAF7"
            selectedTextColor: "#000000"
            horizontalAlignment: TextInput.AlignHCenter
            verticalAlignment: TextInput.AlignVCenter
            readOnly: !control.editable
            validator: control.validator
            inputMethodHints: Qt.ImhFormattedNumbersOnly
        }
        background: Rectangle {
            color: "#FFFFFF"
            border.color: control.activeFocus ? "#5A8FD8" : "#9E9E9E"
            border.width: 1
        }
    }

    component RunTextField: TextField {
        id: control
        implicitWidth: 98
        implicitHeight: 20
        Layout.minimumWidth: 49
        selectByMouse: true
        color: "#000000"
        font.family: root.appFontFamily
        font.weight: root.appNormalFontWeight
        horizontalAlignment: TextInput.AlignHCenter
        verticalAlignment: TextInput.AlignVCenter
        validator: DoubleValidator { bottom: -2147483648; top: 2147483647; decimals: 3; notation: DoubleValidator.StandardNotation }
        background: Rectangle {
            color: control.readOnly ? "#F7F7F7" : "#FFFFFF"
            border.color: "#000000"
            border.width: 1
        }
    }

    component RunButton: AppButton {
        implicitWidth: 110
        implicitHeight: 22
        Layout.minimumWidth: 55
        Layout.preferredWidth: 110
        font.pixelSize: 11
    }

    component RunToggleButton: AppButton {
        id: control
        property string firstText: "单次"
        property string secondText: "连续"
        property bool toggledState: false
        text: toggledState ? secondText : firstText
        implicitWidth: 110
        implicitHeight: 22
        Layout.minimumWidth: 55
        Layout.preferredWidth: 110
        highlighted: toggledState
        onClicked: toggledState = !toggledState
    }

    component PositionGroup: GroupBox {
        id: control
        padding: 8
        label: Label {
            x: (control.width - implicitWidth) / 2
            text: control.title
            color: "#000000"
            font.pixelSize: 11
            font.weight: root.appEmphasisFontWeight
            background: Rectangle { color: "#F4F4F4" }
        }
        background: Rectangle {
            y: 8
            height: parent.height - 8
            color: "#F4F4F4"
            border.color: "#000000"
            border.width: 1
        }
    }

    component PositionRunPanel: Item {
        id: panel
        property int negativeLimit: -200000
        property int positiveLimit: 200000
        property int currentPosition: 0

        SplitView {
            anchors.fill: parent
            orientation: Qt.Horizontal
            handle: Rectangle { implicitWidth: 2; color: SplitHandle.hovered ? "#8E8E8E" : "#B8B8B8" }

            SplitView {
                SplitView.preferredWidth: parent.width * 0.58
                SplitView.minimumWidth: 220
                SplitView.fillHeight: true
                orientation: Qt.Vertical
                handle: Rectangle { implicitHeight: 2; color: SplitHandle.hovered ? "#8E8E8E" : "#B8B8B8" }

                PositionGroup {
                    title: qsTr("step1")
                    SplitView.fillWidth: true
                    SplitView.preferredHeight: parent.height * 0.48
                    SplitView.minimumHeight: 130
                    contentItem: ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 16
                        spacing: 10
                        GridLayout {
                            Layout.fillWidth: true
                            columns: 2
                            columnSpacing: 18
                            rowSpacing: 10
                            Label { text: qsTr("位置点动速度："); color: "#000000"; Layout.alignment: Qt.AlignRight }
                            RunTextField { Layout.fillWidth: true }
                            Label { text: qsTr("位置点动加速度："); color: "#000000"; Layout.alignment: Qt.AlignRight }
                            RunTextField { Layout.fillWidth: true }
                            Label { text: qsTr("位置点动减速度："); color: "#000000"; Layout.alignment: Qt.AlignRight }
                            RunTextField { Layout.fillWidth: true }
                        }
                        Item { Layout.fillHeight: true }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 16
                            RunButton { text: qsTr("使能") }
                            Item { Layout.fillWidth: true }
                            RunButton { text: qsTr("反向") }
                            Item { Layout.fillWidth: true }
                            RunButton { text: qsTr("正向") }
                        }
                    }
                }

                PositionGroup {
                    title: qsTr("step2")
                    SplitView.fillWidth: true
                    SplitView.minimumHeight: 170
                    contentItem: ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 16
                        spacing: 8
                        GridLayout {
                            Layout.fillWidth: true
                            columns: 2
                            columnSpacing: 18
                            rowSpacing: 8
                            Label { text: qsTr("运行距离："); color: "#000000"; Layout.alignment: Qt.AlignRight }
                            RunTextField { Layout.fillWidth: true }
                            Label { text: qsTr("运行速度（rpm）："); color: "#000000"; Layout.alignment: Qt.AlignRight }
                            RunTextField { Layout.fillWidth: true }
                            Label { text: qsTr("运行加速度："); color: "#000000"; Layout.alignment: Qt.AlignRight }
                            RunTextField { Layout.fillWidth: true }
                            Label { text: qsTr("运行减速度："); color: "#000000"; Layout.alignment: Qt.AlignRight }
                            RunTextField { Layout.fillWidth: true }
                            Label { text: qsTr("等待时间："); color: "#000000"; Layout.alignment: Qt.AlignRight }
                            RunTextField { Layout.fillWidth: true }
                        }
                        Item { Layout.preferredHeight: 8; Layout.minimumHeight: 0 }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 14
                            RunToggleButton { firstText: qsTr("单次"); secondText: qsTr("连续") }
                            Item { Layout.fillWidth: true }
                            RunToggleButton { firstText: qsTr("正向"); secondText: qsTr("反向") }
                            Item { Layout.fillWidth: true }
                            RunToggleButton { firstText: qsTr("运行"); secondText: qsTr("暂停") }
                        }
                    }
                }
            }

            PositionGroup {
                title: qsTr("位置动态展示")
                SplitView.minimumWidth: 190
                SplitView.fillHeight: true
                contentItem: ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 20
                    spacing: 12
                    Item { Layout.fillHeight: true }
                    Label { Layout.alignment: Qt.AlignHCenter; text: qsTr("当前位置"); color: "#000000" }
                    RunTextField { Layout.alignment: Qt.AlignHCenter; readOnly: true; text: String(panel.currentPosition); implicitWidth: 117; Layout.minimumWidth: 59 }
                    Slider {
                        id: positionSlider
                        Layout.fillWidth: true
                        from: panel.negativeLimit
                        to: panel.positiveLimit
                        value: panel.currentPosition
                        stepSize: 1
                        enabled: false
                        background: Rectangle {
                            x: positionSlider.leftPadding
                            y: positionSlider.topPadding + positionSlider.availableHeight / 2 - height / 2
                            width: positionSlider.availableWidth
                            height: 4
                            color: "#FFFFFF"
                            border.color: "#000000"
                        }
                        handle: Rectangle {
                            x: positionSlider.leftPadding + positionSlider.visualPosition * (positionSlider.availableWidth - width)
                            y: positionSlider.topPadding + positionSlider.availableHeight / 2 - height / 2
                            width: 8
                            height: 14
                            color: "#000000"
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 24
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            Label { Layout.alignment: Qt.AlignHCenter; text: qsTr("负极限位置"); color: "#000000" }
                            RunTextField {
                                Layout.fillWidth: true
                                text: String(panel.negativeLimit)
                                validator: IntValidator { bottom: -2147483648; top: 2147483647 }
                                onEditingFinished: {
                                    var nextValue = Number(text)
                                    if (!isNaN(nextValue) && nextValue < panel.positiveLimit) {
                                        panel.negativeLimit = nextValue
                                    } else {
                                        text = String(panel.negativeLimit)
                                    }
                                }
                            }
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            Label { Layout.alignment: Qt.AlignHCenter; text: qsTr("正极限位置"); color: "#000000" }
                            RunTextField {
                                Layout.fillWidth: true
                                text: String(panel.positiveLimit)
                                validator: IntValidator { bottom: -2147483648; top: 2147483647 }
                                onEditingFinished: {
                                    var nextValue = Number(text)
                                    if (!isNaN(nextValue) && nextValue > panel.negativeLimit) {
                                        panel.positiveLimit = nextValue
                                    } else {
                                        text = String(panel.positiveLimit)
                                    }
                                }
                            }
                        }
                    }
                    Item { Layout.fillHeight: true }
                }
            }
        }
    }

    component JogRunPanel: Item {
        Rectangle {
            anchors.fill: parent
            color: "#F4F4F4"
            border.color: "#000000"
            Text {
                anchors.centerIn: parent
                text: qsTr("点动运行窗体")
                color: "#000000"
                font.pixelSize: 18
                font.weight: root.appEmphasisFontWeight
            }
        }
    }

    component RunDockPane: Rectangle {
        id: dockPane
        property bool preview: false

        color: preview ? "#EAF4FF" : "#FFFFFF"
        border.color: preview ? "#5A8FD8" : "#000000"
        border.width: preview ? 2 : 1
        clip: true

        Component { id: dockPositionPanel; PositionRunPanel { anchors.fill: parent } }
        Component { id: dockJogPanel; JogRunPanel { anchors.fill: parent } }

        Rectangle {
            id: dockPaneTitleBar
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: 28
            color: preview ? "#DDEBFF" : "#D9F2F0"
            border.color: preview ? "#5A8FD8" : "#7DAAA6"
            property real dragStartX: 0
            property real dragStartY: 0
            property real dragStartWidth: 0
            property real dragStartHeight: 0

            Text {
                anchors.left: parent.left
                anchors.leftMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                text: preview ? qsTr("释放后嵌入：") + root.runWindowTitle : root.runWindowTitle
                color: "#000000"
                font.weight: root.appEmphasisFontWeight
                elide: Text.ElideRight
            }

            HoverHandler { cursorShape: Qt.SizeAllCursor }
            DragHandler {
                id: dockPaneDrag
                target: null
                enabled: !dockPane.preview
                onActiveChanged: {
                    if (active) {
                        var panePoint = root.itemScreenPoint(dockPane, 0, 0)
                        dockPaneTitleBar.dragStartX = panePoint.x
                        dockPaneTitleBar.dragStartY = panePoint.y
                        dockPaneTitleBar.dragStartWidth = dockPane.width
                        dockPaneTitleBar.dragStartHeight = dockPane.height
                        root.runFloatX = panePoint.x
                        root.runFloatY = panePoint.y
                        root.runFloatWidth = Math.max(300, dockPane.width)
                        root.runFloatHeight = Math.max(210, dockPane.height)
                        root.runWindowEmbedded = false
                        root.runDockPreviewEdge = ""
                    } else {
                        var releasePoint = root.itemScreenPoint(dockPaneTitleBar, dockPaneDrag.centroid.position.x, dockPaneDrag.centroid.position.y)
                        root.finishRunWindowDrag(releasePoint.x, releasePoint.y)
                    }
                }
                onTranslationChanged: {
                    if (!active) {
                        return
                    }
                    root.runFloatX = dockPaneTitleBar.dragStartX + translation.x
                    root.runFloatY = dockPaneTitleBar.dragStartY + translation.y
                    var pointerPoint = root.itemScreenPoint(dockPaneTitleBar, dockPaneDrag.centroid.position.x, dockPaneDrag.centroid.position.y)
                    root.updateRunDockPreview(pointerPoint.x, pointerPoint.y)
                }
            }
        }

        Loader {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: dockPaneTitleBar.bottom
            anchors.bottom: parent.bottom
            anchors.margins: 2
            visible: !dockPane.preview
            sourceComponent: root.runWindowMode === "position" ? dockPositionPanel : dockJogPanel
        }

        Text {
            anchors.centerIn: parent
            visible: dockPane.preview
            text: qsTr("松开鼠标嵌入此区域")
            color: "#000000"
            font.pixelSize: 18
            font.weight: root.appEmphasisFontWeight
        }
    }

    component RunToolWindow: Window {
        id: toolWindow
        property string mode: "position"
        property string titleText: ""
        signal moveRequested(real nextX, real nextY, real screenX, real screenY)
        signal dragReleased(real screenX, real screenY)
        signal resizeRequested(real nextWidth, real nextHeight)

        width: 492
        height: 252
        minimumWidth: 300
        minimumHeight: 210
        modality: Qt.NonModal
        flags: Qt.Tool | Qt.FramelessWindowHint
        title: titleText
        color: "#FFFFFF"

        Component { id: floatPositionPanel; PositionRunPanel { anchors.fill: parent } }
        Component { id: floatJogPanel; JogRunPanel { anchors.fill: parent } }

        Rectangle {
            anchors.fill: parent
            color: "#FFFFFF"
            border.color: "#000000"
            clip: true

            Rectangle {
                id: floatTitleBar
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                height: 28
                color: "#D9F2F0"
                border.color: "#7DAAA6"
                property real dragStartX: 0
                property real dragStartY: 0

                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.right: parent.right
                    anchors.rightMargin: 8
                    text: toolWindow.titleText
                    color: "#000000"
                    font.weight: root.appEmphasisFontWeight
                    elide: Text.ElideRight
                }

                HoverHandler { cursorShape: Qt.SizeAllCursor }
                DragHandler {
                    id: floatWindowDrag
                    target: null
                    onActiveChanged: {
                        if (active) {
                            floatTitleBar.dragStartX = toolWindow.x
                            floatTitleBar.dragStartY = toolWindow.y
                        } else {
                            var releaseScreenX = toolWindow.x + floatWindowDrag.centroid.position.x
                            var releaseScreenY = toolWindow.y + floatWindowDrag.centroid.position.y
                            toolWindow.dragReleased(releaseScreenX, releaseScreenY)
                        }
                    }
                    onTranslationChanged: {
                        if (!active) {
                            return
                        }
                        var nextX = floatTitleBar.dragStartX + translation.x
                        var nextY = floatTitleBar.dragStartY + translation.y
                        var pointerX = nextX + floatWindowDrag.centroid.position.x
                        var pointerY = nextY + floatWindowDrag.centroid.position.y
                        toolWindow.moveRequested(nextX, nextY, pointerX, pointerY)
                    }
                }
            }

            Loader {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: floatTitleBar.bottom
                anchors.bottom: parent.bottom
                anchors.margins: 2
                sourceComponent: toolWindow.mode === "position" ? floatPositionPanel : floatJogPanel
            }

            Rectangle {
                id: floatWindowResizeHandle
                width: 14
                height: 14
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                color: resizeHover.hovered || resizeDrag.active ? "#7DAAA6" : "#D0D0D0"
                property real startWidth: 0
                property real startHeight: 0

                HoverHandler { id: resizeHover; cursorShape: Qt.SizeFDiagCursor }
                DragHandler {
                    id: resizeDrag
                    target: null
                    onActiveChanged: {
                        if (active) {
                            floatWindowResizeHandle.startWidth = toolWindow.width
                            floatWindowResizeHandle.startHeight = toolWindow.height
                        }
                    }
                    onTranslationChanged: {
                        if (!active) {
                            return
                        }
                        toolWindow.resizeRequested(Math.max(toolWindow.minimumWidth, floatWindowResizeHandle.startWidth + translation.x),
                                                   Math.max(toolWindow.minimumHeight, floatWindowResizeHandle.startHeight + translation.y))
                    }
                }
            }
        }
    }

    function setParameterColumnWidth(column, width) {
        var nextWidths = parameterColumnWidths.slice(0)
        var boundedWidth = Math.max(parameterColumnMinWidths[column], Math.round(width))
        if (nextWidths[column] === boundedWidth) {
            return
        }
        nextWidths[column] = boundedWidth
        parameterColumnWidths = nextWidths
        rebuildParameterEffectiveWidths()
    }

    function rebuildParameterEffectiveWidths() {
        var effectiveWidths = parameterColumnWidths.slice(0)
        var availableWidth = Math.floor(parameterViewportWidth)
        if (availableWidth > 0 && effectiveWidths.length > 0) {
            var totalWidth = 0
            for (var i = 0; i < effectiveWidths.length; ++i) {
                totalWidth += effectiveWidths[i]
            }

            if (totalWidth < availableWidth) {
                effectiveWidths[effectiveWidths.length - 1] += availableWidth - totalWidth
            } else if (totalWidth > availableWidth) {
                var overflowWidth = totalWidth - availableWidth
                for (var rightIndex = effectiveWidths.length - 1; rightIndex >= 0 && overflowWidth > 0; --rightIndex) {
                    var reducibleWidth = Math.max(0, effectiveWidths[rightIndex] - parameterColumnMinWidths[rightIndex])
                    var reduceWidth = Math.min(reducibleWidth, overflowWidth)
                    effectiveWidths[rightIndex] -= reduceWidth
                    overflowWidth -= reduceWidth
                }

                for (var forcedIndex = effectiveWidths.length - 1; forcedIndex >= 0 && overflowWidth > 0; --forcedIndex) {
                    var hardMinimumWidth = 24
                    var forcedReducibleWidth = Math.max(0, effectiveWidths[forcedIndex] - hardMinimumWidth)
                    var forcedReduceWidth = Math.min(forcedReducibleWidth, overflowWidth)
                    effectiveWidths[forcedIndex] -= forcedReduceWidth
                    overflowWidth -= forcedReduceWidth
                }
            }
        }

        parameterColumnEffectiveWidths = effectiveWidths
        if (parameterTable) {
            parameterTable.forceLayout()
            parameterTable.clampHorizontalScroll()
        }
    }

    function preferredParameterTableWidth() {
        var total = 0
        for (var i = 0; i < parameterColumnWidths.length; ++i) {
            total += parameterColumnWidths[i]
        }
        return total
    }

    function minimumParameterTableWidth() {
        var total = 0
        for (var i = 0; i < parameterColumnMinWidths.length; ++i) {
            total += parameterColumnMinWidths[i]
        }
        return total
    }

    function parameterColumnWidth(column) {
        return parameterColumnEffectiveWidths[column]
    }

    function parameterTableWidth() {
        var total = 0
        for (var i = 0; i < parameterColumnEffectiveWidths.length; ++i) {
            total += parameterColumnWidth(i)
        }
        return total
    }

    function proportionalColumnWidth(totalWidth, ratios, column) {
        var boundedWidth = Math.max(0, Math.floor(totalWidth))
        if (boundedWidth <= 0 || column < 0 || column >= ratios.length) {
            return 80
        }
        if (column === ratios.length - 1) {
            var usedWidth = 0
            for (var i = 0; i < ratios.length - 1; ++i) {
                usedWidth += Math.floor(boundedWidth * ratios[i])
            }
            return Math.max(24, boundedWidth - usedWidth)
        }
        return Math.max(24, Math.floor(boundedWidth * ratios[column]))
    }

    function mainContentRect() {
        var point = mainWorkspace.mapToItem(appSurface, 0, 0)
        return { x: point.x, y: point.y, width: mainWorkspace.width, height: mainWorkspace.height }
    }

    function itemScreenPoint(item, itemX, itemY) {
        if (item.mapToGlobal) {
            var globalPoint = item.mapToGlobal(itemX, itemY)
            return { x: globalPoint.x, y: globalPoint.y }
        }

        var localPoint = item.mapToItem(appSurface, itemX, itemY)
        return { x: root.x + localPoint.x, y: root.y + localPoint.y }
    }

    function mainContentScreenRect() {
        var point = itemScreenPoint(mainWorkspace, 0, 0)
        return { x: point.x, y: point.y, width: mainWorkspace.width, height: mainWorkspace.height }
    }

    function runDockRect() {
        var dockItem = effectiveRunDockEdge === "left" || effectiveRunDockEdge === "top" ? runDockBefore : runDockAfter
        var point = dockItem.mapToItem(appSurface, 0, 0)
        return { x: point.x, y: point.y, width: dockItem.width, height: dockItem.height }
    }

    function dockedRunX() {
        return runDockRect().x
    }

    function dockedRunY() {
        return runDockRect().y
    }

    function dockedRunWidth() {
        return runDockRect().width
    }

    function dockedRunHeight() {
        return runDockRect().height
    }

    function openRunWindow(mode) {
        runWindowMode = mode
        runWindowTitle = mode === "position" ? qsTr("定位运行") : qsTr("点动运行")
        runWindowVisible = true
        runDockPreviewEdge = ""
        if (!runWindowEmbedded) {
            var area = mainContentScreenRect()
            runFloatX = Math.max(0, area.x + (area.width - runFloatWidth) / 2)
            runFloatY = Math.max(0, area.y + (area.height - runFloatHeight) / 2)
            Qt.callLater(function() { runToolWindow.requestActivate() })
        }
    }

    function dockRunWindow(edge) {
        runDockEdge = edge
        runDockFraction = 0.5
        runWindowEmbedded = true
        runDockPreviewEdge = ""
        runWindowVisible = true
    }

    function floatRunWindow() {
        var area = mainContentScreenRect()
        runWindowEmbedded = false
        runDockPreviewEdge = ""
        runWindowVisible = true
        runFloatX = Math.max(0, area.x + 40)
        runFloatY = Math.max(0, area.y + 40)
        Qt.callLater(function() { runToolWindow.requestActivate() })
    }

    function dockEdgeForScreenPoint(screenX, screenY) {
        var area = mainContentScreenRect()
        if (screenX < area.x || screenX > area.x + area.width || screenY < area.y || screenY > area.y + area.height) {
            return ""
        }

        var edgeMargin = Math.min(120, Math.max(48, Math.min(area.width, area.height) * 0.12))
        var topDistance = screenY - area.y
        var bottomDistance = area.y + area.height - screenY
        var leftDistance = screenX - area.x
        var rightDistance = area.x + area.width - screenX
        var nearestDistance = Math.min(topDistance, bottomDistance, leftDistance, rightDistance)
        if (nearestDistance > edgeMargin) {
            return ""
        }

        if (nearestDistance === topDistance) {
            return "top"
        }
        if (nearestDistance === bottomDistance) {
            return "bottom"
        }
        if (nearestDistance === leftDistance) {
            return "left"
        }
        return "right"
    }

    function updateRunDockPreview(screenX, screenY) {
        if (runWindowEmbedded || !runWindowVisible) {
            return
        }

        var edge = dockEdgeForScreenPoint(screenX, screenY)
        if (runDockPreviewEdge !== edge) {
            runDockPreviewEdge = edge
        }
    }

    function finishRunWindowDrag(screenX, screenY) {
        var edge = dockEdgeForScreenPoint(screenX, screenY)
        if (edge.length > 0) {
            dockRunWindow(edge)
        } else {
            runDockPreviewEdge = ""
        }
    }

    Connections {
        target: appController
        function onToastRequested(message) {
            toast.show(message)
        }
    }

    Rectangle {
        id: appSurface
        anchors.fill: parent
        color: "#FFFFFF"

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            SplitView {
                id: workSplit
                Layout.fillWidth: true
                Layout.fillHeight: true
                orientation: Qt.Vertical
                handle: Rectangle {
                    implicitHeight: 6
                    color: SplitHandle.hovered ? "#7DAAA6" : "#A8A8A8"
                }

                Rectangle {
                    visible: !root.contentOnly
                    SplitView.preferredHeight: root.contentOnly ? 0 : 52
                    SplitView.minimumHeight: root.contentOnly ? 0 : 44
                    SplitView.maximumHeight: root.contentOnly ? 0 : 76
                    color: "#D9F2F0"
                    border.color: "#7DAAA6"

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        spacing: 8

                        AppButton {
                            text: qsTr("通讯设置")
                            onClicked: {
                                appController.refreshSerialPorts()
                                communicationDialog.open()
                            }
                        }
                        AppButton {
                            text: appController.connected ? qsTr("断开连接") : qsTr("连接/断开")
                            onClicked: appController.toggleConnection()
                        }
                        Item { Layout.fillWidth: true }
                        AppButton {
                            text: qsTr("保存参数")
                            onClicked: saveParameterMenu.open()
                            Menu {
                                id: saveParameterMenu
                                y: parent.height
                                width: parent.width + 18
                                background: Rectangle { color: "#FFFFFF"; border.color: "#A8A8A8" }
                                MenuItem {
                                    id: saveUserMenuItem
                                    text: qsTr("用户参数")
                                    contentItem: Text { text: saveUserMenuItem.text; color: "#000000"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                    background: Rectangle { color: saveUserMenuItem.highlighted ? "#DDEAF7" : "#FFFFFF" }
                                    onTriggered: appController.saveUserParameters()
                                }
                                MenuItem {
                                    id: saveMotorMenuItem
                                    text: qsTr("电机参数")
                                    contentItem: Text { text: saveMotorMenuItem.text; color: "#000000"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                    background: Rectangle { color: saveMotorMenuItem.highlighted ? "#DDEAF7" : "#FFFFFF" }
                                    onTriggered: appController.saveMotorParameters()
                                }
                            }
                        }
                        AppButton { text: qsTr("开环调试"); onClicked: toast.show(qsTr("开环调试将在后续 QML 阶段接入")) }
                        AppButton { text: qsTr("电机调零"); onClicked: toast.show(qsTr("电机调零将在后续 QML 阶段接入")) }
                        AppButton { text: qsTr("故障复位"); onClicked: appController.sendFaultResetCommand() }
                        AppButton { text: qsTr("恢复出厂"); onClicked: appController.requestFactoryResetCommand() }
                        AppButton { text: qsTr("电机复位"); onClicked: appController.requestMotorResetCommand() }
                    }
                }

                SplitView {
                    id: contentSplit
                    SplitView.fillHeight: true
                    orientation: Qt.Horizontal
                    handle: Rectangle {
                        implicitWidth: 6
                        color: SplitHandle.hovered ? "#7DAAA6" : "#A8A8A8"
                    }

                    Rectangle {
                        visible: !root.contentOnly
                        SplitView.preferredWidth: root.contentOnly ? 0 : 220
                        SplitView.minimumWidth: root.contentOnly ? 0 : 170
                        SplitView.maximumWidth: root.contentOnly ? 0 : 100000
                        color: "#FFF8DA"
                        border.color: "#C8B56A"

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 10
                            spacing: 10

                            Label {
                                text: qsTr("选择电机型号")
                                color: "#000000"
                                font.weight: root.appEmphasisFontWeight
                            }

                            AppComboBox {
                                id: modelSelector
                                Layout.fillWidth: true
                                model: appController.modelNames
                                currentIndex: appController.currentModelIndex
                                onActivated: appController.currentModelIndex = index
                                Connections {
                                    target: appController
                                    function onCurrentModelIndexChanged() {
                                        modelSelector.currentIndex = appController.currentModelIndex
                                    }
                                }
                            }

                            Label {
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                                color: "#000000"
                                text: qsTr("型号下方可选择总表和运行工具")
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 4
                                Repeater {
                                    model: ["参数总表", "监控总表", "故障总表", "示波器", "点动运行", "定位运行"]
                                    delegate: AppButton {
                                        required property int index
                                        required property string modelData
                                        Layout.fillWidth: true
                                        text: modelData
                                        highlighted: root.activePage === index
                                        onClicked: {
                                            if (index === 4) {
                                                uiController.showJogDock()
                                            } else if (index === 5) {
                                                uiController.showPositionDock()
                                            } else {
                                                root.activePage = index
                                                appController.setActivePage(index)
                                            }
                                        }
                                    }
                                }
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 1
                                color: "#D6C98A"
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                visible: root.activePage === 0
                                spacing: 6

                                Label {
                                    text: qsTr("参数操作")
                                    color: "#000000"
                                    font.weight: root.appEmphasisFontWeight
                                }
                                AppButton { Layout.fillWidth: true; text: qsTr("上传全部"); onClicked: appController.uploadAllParameters() }
                                AppButton { Layout.fillWidth: true; text: qsTr("下载全部"); onClicked: appController.downloadAllParameters() }
                                AppButton { Layout.fillWidth: true; text: qsTr("上传勾选"); onClicked: appController.uploadCheckedParameters() }
                                AppButton { Layout.fillWidth: true; text: qsTr("下载勾选"); onClicked: appController.downloadCheckedParameters() }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                visible: root.activePage === 1
                                spacing: 6

                                Label {
                                    text: qsTr("监控操作")
                                    color: "#000000"
                                    font.weight: root.appEmphasisFontWeight
                                }
                                Label { text: qsTr("监控间隔 ms"); color: "#000000" }
                                AppSpinBox {
                                    Layout.fillWidth: true
                                    from: 10
                                    to: 60000
                                    stepSize: 10
                                    value: appController.monitorIntervalMs
                                    onValueModified: appController.setMonitorIntervalMs(value)
                                }
                                AppButton {
                                    Layout.fillWidth: true
                                    text: appController.monitorPollingActive ? qsTr("关闭监控") : qsTr("启动监控")
                                    highlighted: appController.monitorPollingActive
                                    onClicked: appController.toggleMonitorPolling()
                                }
                            }

                            Item { Layout.fillHeight: true }
                        }
                    }

                    Item {
                        id: mainWorkspace
                        SplitView.fillWidth: true
                        SplitView.fillHeight: true

                        SplitView {
                            id: mainWorkspaceSplit
                            anchors.fill: parent
                            orientation: root.effectiveRunDockEdge === "left" || root.effectiveRunDockEdge === "right" ? Qt.Horizontal : Qt.Vertical
                            handle: Rectangle {
                                implicitWidth: 4
                                implicitHeight: 4
                                color: SplitHandle.hovered ? "#7DAAA6" : "#B8B8B8"
                            }

                            Item {
                                id: runDockBefore
                                visible: root.runDockSlotVisible && (root.effectiveRunDockEdge === "left" || root.effectiveRunDockEdge === "top")
                                SplitView.preferredWidth: visible && mainWorkspaceSplit.orientation === Qt.Horizontal ? mainWorkspace.width * root.runDockFraction : 0
                                SplitView.preferredHeight: visible && mainWorkspaceSplit.orientation === Qt.Vertical ? mainWorkspace.height * root.runDockFraction : 0
                                SplitView.minimumWidth: visible && mainWorkspaceSplit.orientation === Qt.Horizontal ? 260 : 0
                                SplitView.minimumHeight: visible && mainWorkspaceSplit.orientation === Qt.Vertical ? 190 : 0

                                RunDockPane {
                                    anchors.fill: parent
                                    preview: root.runDockPreviewEdge.length > 0
                                }
                            }

                            StackLayout {
                                id: pageStack
                                SplitView.fillWidth: true
                                SplitView.fillHeight: true
                                SplitView.minimumWidth: 300
                                SplitView.minimumHeight: 220
                                currentIndex: root.activePage

                        Rectangle {
                            color: "#CDE8B7"

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 12
                                spacing: 8

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 12

                                    Label {
                                        text: qsTr("参数总表")
                                        color: "#000000"
                                        font.pixelSize: 18
                                        font.weight: root.appEmphasisFontWeight
                                    }
                                    TextField {
                                        id: parameterSearch
                                        Layout.preferredWidth: 280
                                        placeholderText: qsTr("搜索功能说明，回车定位")
                                        selectByMouse: true
                                        color: "#000000"
                                        onAccepted: {
                                            var row = appController.parameterModel.findNextFunctionRow(text, root.lastParameterMatchedRow)
                                            if (row >= 0) {
                                                root.lastParameterMatchedRow = row
                                                parameterTable.positionViewAtRow(row, TableView.AlignTop)
                                                parameterTable.selectedRow = row
                                            } else {
                                                root.lastParameterMatchedRow = -1
                                                toast.show(qsTr("未找到匹配参数"))
                                            }
                                        }
                                    }
                                    Item { Layout.fillWidth: true }
                                }

                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    color: "#FFFFFF"
                                    border.color: "#B8B8B8"
                                    clip: true

                                    ColumnLayout {
                                        anchors.fill: parent
                                        spacing: 0

                                        Item {
                                            Layout.fillWidth: true
                                            Layout.preferredHeight: 32
                                            clip: true

                                            Row {
                                                id: parameterHeaderRow
                                                x: -parameterTable.contentX
                                                width: root.parameterTableWidth()
                                                height: parent.height
                                                Repeater {
                                                    model: root.parameterColumnTitles.length
                                                    delegate: Rectangle {
                                                        required property int index
                                                        width: root.parameterColumnWidth(index)
                                                        height: parameterHeaderRow.height
                                                        color: index === 0 ? "#EFEFEF" : "#F2F2F2"
                                                        border.color: "#B8B8B8"
                                                        Text {
                                                            anchors.centerIn: parent
                                                            text: root.parameterColumnTitles[index]
                                                            color: "#000000"
                                                            font.weight: root.appEmphasisFontWeight
                                                            elide: Text.ElideRight
                                                        }
                                                        Rectangle {
                                                            id: resizeHandle
                                                            anchors.right: parent.right
                                                            width: 6
                                                            height: parent.height
                                                            color: resizeHover.hovered || resizeDrag.active ? "#7DAAA6" : "transparent"
                                                            property real pressWidth: 0
                                                            HoverHandler {
                                                                id: resizeHover
                                                                cursorShape: Qt.SplitHCursor
                                                            }
                                                            DragHandler {
                                                                id: resizeDrag
                                                                target: null
                                                                xAxis.enabled: true
                                                                yAxis.enabled: false
                                                                onActiveChanged: {
                                                                    if (active) {
                                                                        resizeHandle.pressWidth = root.parameterColumnWidth(index)
                                                                    }
                                                                }
                                                                onTranslationChanged: {
                                                                    if (active) {
                                                                        root.setParameterColumnWidth(index, resizeHandle.pressWidth + translation.x)
                                                                    }
                                                                }
                                                            }
                                                        }
                                                    }
                                                }
                                            }
                                        }

                                        TableView {
                                            id: parameterTable
                                            Layout.fillWidth: true
                                            Layout.fillHeight: true
                                            clip: true
                                            model: appController.parameterModel
                                            rowHeightProvider: function(row) { return 30 }
                                            columnWidthProvider: function(column) { return root.parameterColumnWidth(column) }
                                            contentWidth: root.parameterTableWidth()
                                            boundsBehavior: Flickable.StopAtBounds
                                            ScrollBar.horizontal: ScrollBar { }
                                            ScrollBar.vertical: ScrollBar { }
                                            property int selectedRow: -1
                                            function clampHorizontalScroll() {
                                                contentX = Math.max(0, Math.min(contentX, Math.max(0, contentWidth - width)))
                                            }
                                            onWidthChanged: {
                                                root.parameterViewportWidth = width
                                                root.rebuildParameterEffectiveWidths()
                                                clampHorizontalScroll()
                                            }
                                            onContentWidthChanged: clampHorizontalScroll()
                                            Component.onCompleted: {
                                                root.parameterViewportWidth = width
                                                root.rebuildParameterEffectiveWidths()
                                            }

                                            delegate: Rectangle {
                                                id: cell
                                                required property int row
                                                required property int column
                                                required property var display
                                                required property bool rowChecked
                                                required property string rawValue
                                                required property bool editable
                                                required property bool pendingSend
                                                required property string backgroundColor
                                                required property bool comboBox
                                                required property var comboOptions

                                                implicitWidth: root.parameterColumnWidth(column)
                                                implicitHeight: 30
                                                color: pendingSend && column === 3 ? backgroundColor : (parameterTable.selectedRow === row ? "#DDEAF7" : backgroundColor)
                                                border.color: "#B8B8B8"

                                                onRawValueChanged: {
                                                    if (valueEditor && !valueEditor.activeFocus) {
                                                        valueEditor.text = cell.rawValue
                                                    }
                                                    if (comboEditor && comboEditor.visible && comboEditor.contentItem && !comboEditor.contentItem.activeFocus) {
                                                        comboEditor.editText = cell.display
                                                    }
                                                }

                                                onDisplayChanged: {
                                                    if (comboEditor && comboEditor.visible && comboEditor.contentItem && !comboEditor.contentItem.activeFocus) {
                                                        comboEditor.editText = cell.display
                                                    }
                                                }

                                                AppCheckBox {
                                                    anchors.centerIn: parent
                                                    visible: cell.column === 0
                                                    checked: cell.rowChecked
                                                    onToggled: appController.setParameterChecked(cell.row, checked)
                                                }

                                                AppComboBox {
                                                    id: comboEditor
                                                    anchors.fill: parent
                                                    anchors.margins: 2
                                                    visible: cell.column === 3 && cell.editable && cell.comboBox
                                                    editable: true
                                                    fieldColor: cell.pendingSend ? "#FFF4B8" : "#FFFFFF"
                                                    displayTextOverride: cell.display
                                                    model: cell.comboOptions
                                                    currentIndex: find(cell.display)
                                                    Component.onCompleted: editText = cell.display
                                                    onVisibleChanged: {
                                                        if (visible) {
                                                            editText = cell.display
                                                        }
                                                    }
                                                    onActivated: appController.editParameterLocal(cell.row, currentText)
                                                    onEnterPressed: function(text) {
                                                        appController.submitParameterValue(cell.row, text)
                                                    }
                                                    onEditingFinishedText: function(text) {
                                                        if (text !== cell.rawValue && text !== cell.display) {
                                                            appController.editParameterLocal(cell.row, text)
                                                        }
                                                    }
                                                }

                                                TextField {
                                                    id: valueEditor
                                                    anchors.fill: parent
                                                    anchors.margins: 2
                                                    visible: cell.column === 3 && cell.editable && !cell.comboBox
                                                    text: cell.rawValue
                                                    color: "#000000"
                                                    horizontalAlignment: TextInput.AlignHCenter
                                                    verticalAlignment: TextInput.AlignVCenter
                                                    selectByMouse: true
                                                    property bool submitted: false
                                                    background: Rectangle {
                                                        color: "transparent"
                                                        border.color: valueEditor.activeFocus ? "#5A8FD8" : "transparent"
                                                    }
                                                    onAccepted: {
                                                        submitted = true
                                                        appController.submitParameterValue(cell.row, text)
                                                        focus = false
                                                    }
                                                    onActiveFocusChanged: {
                                                        if (activeFocus) {
                                                            submitted = false
                                                            parameterTable.selectedRow = cell.row
                                                            return
                                                        }
                                                        if (!submitted && text !== cell.rawValue) {
                                                            appController.editParameterLocal(cell.row, text)
                                                        }
                                                    }
                                                }

                                                Text {
                                                    anchors.fill: parent
                                                    anchors.leftMargin: cell.column === 2 ? 8 : 4
                                                    anchors.rightMargin: 4
                                                    verticalAlignment: Text.AlignVCenter
                                                    horizontalAlignment: cell.column === 2 ? Text.AlignLeft : Text.AlignHCenter
                                                    visible: cell.column !== 0 && !(cell.column === 3 && cell.editable)
                                                    text: cell.display
                                                    color: "#000000"
                                                    elide: Text.ElideRight
                                                }

                                                MouseArea {
                                                    anchors.fill: parent
                                                    acceptedButtons: Qt.LeftButton
                                                    enabled: cell.column !== 0 && !(cell.column === 3 && cell.editable)
                                                    onClicked: parameterTable.selectedRow = cell.row
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        Rectangle {
                            color: "#CDE8B7"
                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 12
                                spacing: 8

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 12
                                    Label { text: qsTr("监控总表"); color: "#000000"; font.pixelSize: 18; font.weight: root.appEmphasisFontWeight }
                                    TextField {
                                        id: monitorSearch
                                        Layout.preferredWidth: 280
                                        placeholderText: qsTr("搜索监控名称，回车定位")
                                        selectByMouse: true
                                        color: "#000000"
                                        onAccepted: {
                                            var row = appController.monitorModel.findNextNameRow(text, root.lastMonitorMatchedRow)
                                            if (row >= 0) {
                                                root.lastMonitorMatchedRow = row
                                                monitorTable.positionViewAtRow(row, TableView.AlignTop)
                                                monitorTable.selectedRow = row
                                            } else {
                                                root.lastMonitorMatchedRow = -1
                                                toast.show(qsTr("未找到匹配监控项"))
                                            }
                                        }
                                    }
                                    Item { Layout.fillWidth: true }
                                }

                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    color: "#FFFFFF"
                                    border.color: "#B8B8B8"
                                    clip: true
                                    ColumnLayout {
                                        anchors.fill: parent
                                        spacing: 0
                                        Item {
                                            Layout.fillWidth: true
                                            Layout.preferredHeight: 32
                                            Row {
                                                id: monitorHeaderRow
                                                width: monitorTable.width
                                                height: parent.height
                                                Repeater {
                                                    model: root.monitorColumnTitles.length
                                                    delegate: Rectangle {
                                                        required property int index
                                                        width: root.proportionalColumnWidth(monitorTable.width, root.monitorColumnRatios, index)
                                                        height: monitorHeaderRow.height
                                                        color: index === 0 ? "#EFEFEF" : "#F2F2F2"
                                                        border.color: "#B8B8B8"
                                                        Text { anchors.centerIn: parent; text: root.monitorColumnTitles[index]; color: "#000000"; font.weight: root.appEmphasisFontWeight; elide: Text.ElideRight }
                                                    }
                                                }
                                            }
                                        }

                                        TableView {
                                            id: monitorTable
                                            Layout.fillWidth: true
                                            Layout.fillHeight: true
                                            clip: true
                                            model: appController.monitorModel
                                            rowHeightProvider: function(row) { return 30 }
                                            columnWidthProvider: function(column) { return root.proportionalColumnWidth(width, root.monitorColumnRatios, column) }
                                            contentWidth: width
                                            boundsBehavior: Flickable.StopAtBounds
                                            ScrollBar.vertical: ScrollBar { }
                                            property int selectedRow: -1

                                            delegate: Rectangle {
                                                id: monitorCell
                                                required property int row
                                                required property int column
                                                required property bool rowChecked
                                                required property string address
                                                required property string nameText
                                                required property string valueText
                                                required property string unitText
                                                required property string remarkText
                                                required property string backgroundColor
                                                implicitWidth: root.proportionalColumnWidth(monitorTable.width, root.monitorColumnRatios, column)
                                                implicitHeight: 30
                                                color: monitorTable.selectedRow === row ? "#DDEAF7" : backgroundColor
                                                border.color: "#B8B8B8"

                                                AppCheckBox {
                                                    anchors.centerIn: parent
                                                    visible: monitorCell.column === 0
                                                    checked: monitorCell.rowChecked
                                                    onToggled: appController.setMonitorChecked(monitorCell.row, checked)
                                                }

                                                Text {
                                                    anchors.fill: parent
                                                    anchors.leftMargin: monitorCell.column === 2 ? 8 : 4
                                                    anchors.rightMargin: 4
                                                    verticalAlignment: Text.AlignVCenter
                                                    horizontalAlignment: monitorCell.column === 2 || monitorCell.column === 5 ? Text.AlignLeft : Text.AlignHCenter
                                                    visible: monitorCell.column !== 0
                                                    text: monitorCell.column === 1 ? monitorCell.address
                                                          : monitorCell.column === 2 ? monitorCell.nameText
                                                          : monitorCell.column === 3 ? monitorCell.valueText
                                                          : monitorCell.column === 4 ? monitorCell.unitText
                                                          : monitorCell.remarkText
                                                    color: "#000000"
                                                    elide: Text.ElideRight
                                                }

                                                MouseArea { anchors.fill: parent; enabled: monitorCell.column !== 0; onClicked: monitorTable.selectedRow = monitorCell.row }
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        Rectangle {
                            color: "#CDE8B7"
                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 12
                                spacing: 8
                                Label { text: qsTr("故障总表"); color: "#000000"; font.pixelSize: 18; font.weight: root.appEmphasisFontWeight }

                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    color: "#FFFFFF"
                                    border.color: "#B8B8B8"
                                    clip: true
                                    ColumnLayout {
                                        anchors.fill: parent
                                        spacing: 0
                                        Item {
                                            Layout.fillWidth: true
                                            Layout.preferredHeight: 32
                                            Row {
                                                id: faultHeaderRow
                                                width: faultTable.width
                                                height: parent.height
                                                Repeater {
                                                    model: root.faultColumnTitles.length
                                                    delegate: Rectangle {
                                                        required property int index
                                                        width: root.proportionalColumnWidth(faultTable.width, root.faultColumnRatios, index)
                                                        height: faultHeaderRow.height
                                                        color: "#F2F2F2"
                                                        border.color: "#B8B8B8"
                                                        Text { anchors.centerIn: parent; text: root.faultColumnTitles[index]; color: "#000000"; font.weight: root.appEmphasisFontWeight; elide: Text.ElideRight }
                                                    }
                                                }
                                            }
                                        }

                                        TableView {
                                            id: faultTable
                                            Layout.fillWidth: true
                                            Layout.fillHeight: true
                                            clip: true
                                            model: appController.faultModel
                                            rowHeightProvider: function(row) { return 30 }
                                            columnWidthProvider: function(column) { return root.proportionalColumnWidth(width, root.faultColumnRatios, column) }
                                            contentWidth: width
                                            boundsBehavior: Flickable.StopAtBounds
                                            ScrollBar.vertical: ScrollBar { }
                                            property int selectedRow: -1

                                            delegate: Rectangle {
                                                id: faultCell
                                                required property int row
                                                required property int column
                                                required property string address
                                                required property string bitText
                                                required property string nameText
                                                required property string valueText
                                                required property string remarkText
                                                required property bool faultActive
                                                required property string backgroundColor
                                                implicitWidth: root.proportionalColumnWidth(faultTable.width, root.faultColumnRatios, column)
                                                implicitHeight: 30
                                                color: faultTable.selectedRow === row ? "#DDEAF7" : backgroundColor
                                                border.color: "#B8B8B8"

                                                Text {
                                                    anchors.fill: parent
                                                    anchors.leftMargin: faultCell.column === 2 || faultCell.column === 4 ? 8 : 4
                                                    anchors.rightMargin: 4
                                                    verticalAlignment: Text.AlignVCenter
                                                    horizontalAlignment: faultCell.column === 2 || faultCell.column === 4 ? Text.AlignLeft : Text.AlignHCenter
                                                    text: faultCell.column === 0 ? faultCell.address
                                                          : faultCell.column === 1 ? faultCell.bitText
                                                          : faultCell.column === 2 ? faultCell.nameText
                                                          : faultCell.column === 3 ? faultCell.valueText
                                                          : faultCell.remarkText
                                                    color: "#000000"
                                                    elide: Text.ElideRight
                                                }

                                                MouseArea { anchors.fill: parent; onClicked: faultTable.selectedRow = faultCell.row }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                        Rectangle {
                            color: "#CDE8B7"
                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 12
                                spacing: 8
                                Label { text: qsTr("示波器"); color: "#000000"; font.pixelSize: 18; font.weight: root.appEmphasisFontWeight }
                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    color: "#F4F4F4"
                                    border.color: "#B8B8B8"
                                    Text { anchors.centerIn: parent; text: qsTr("示波器窗体"); color: "#000000"; font.pixelSize: 22 }
                                }
                            }
                        }
                        Rectangle { color: "#CDE8B7"; Text { anchors.centerIn: parent; text: qsTr("点动运行窗体将在后续 QML 阶段接入"); color: "#000000"; font.pixelSize: 22 } }
                        Rectangle {
                            color: "#CDE8B7"
                            Text {
                                anchors.centerIn: parent
                                text: qsTr("定位运行窗体将在后续 QML 阶段接入")
                                color: "#000000"
                                font.pixelSize: 22
                            }
                        }
                    }

                            Item {
                                id: runDockAfter
                                visible: root.runDockSlotVisible && (root.effectiveRunDockEdge === "right" || root.effectiveRunDockEdge === "bottom")
                                SplitView.preferredWidth: visible && mainWorkspaceSplit.orientation === Qt.Horizontal ? mainWorkspace.width * root.runDockFraction : 0
                                SplitView.preferredHeight: visible && mainWorkspaceSplit.orientation === Qt.Vertical ? mainWorkspace.height * root.runDockFraction : 0
                                SplitView.minimumWidth: visible && mainWorkspaceSplit.orientation === Qt.Horizontal ? 260 : 0
                                SplitView.minimumHeight: visible && mainWorkspaceSplit.orientation === Qt.Vertical ? 190 : 0

                                RunDockPane {
                                    anchors.fill: parent
                                    preview: root.runDockPreviewEdge.length > 0
                                }
                            }
                        }
                    }
                }
            }

            Rectangle {
                visible: !root.contentOnly
                Layout.fillWidth: true
                Layout.preferredHeight: root.contentOnly ? 0 : 28
                color: "#FADDE1"
                border.color: "#C78B91"

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
                    spacing: 16

                    Label { text: appController.connectionStatus; color: "#000000" }
                    Label { text: appController.selectedModelStatus; color: "#000000" }
                    Rectangle {
                        Layout.preferredHeight: 22
                        implicitWidth: servoStateLabel.implicitWidth + 12
                        color: appController.servoAlarmActive ? "#FF4D4F" : "transparent"
                        border.color: appController.servoAlarmActive ? "#C00000" : "transparent"
                        Label {
                            id: servoStateLabel
                            anchors.centerIn: parent
                            text: appController.servoStateText
                            color: "#000000"
                        }
                    }
                    Item { Layout.fillWidth: true }
                    ProgressBar {
                        Layout.preferredWidth: 320
                        from: 0
                        to: appController.progressMaximum
                        value: appController.progressValue
                        background: Rectangle { color: "#FFFFFF"; border.color: "#9E9E9E" }
                        contentItem: Item {
                            Rectangle {
                                width: parent.width * (appController.progressMaximum > 0 ? appController.progressValue / appController.progressMaximum : 0)
                                height: parent.height
                                color: "#00B050"
                            }
                            Text {
                                anchors.centerIn: parent
                                text: appController.progressText
                                color: "#000000"
                                font.pixelSize: 11
                            }
                        }
                    }
                    Label { text: appController.operationStatus; color: "#000000" }
                }
            }
        }

        Item {
            id: runOverlay
            anchors.fill: parent
            z: 20
            visible: false

            Rectangle {
                id: collapsedRunTab
                visible: false
                width: root.runDockEdge === "left" || root.runDockEdge === "right" ? 26 : 150
                height: root.runDockEdge === "top" || root.runDockEdge === "bottom" ? 26 : 150
                x: root.runDockEdge === "right" ? root.mainContentRect().x + root.mainContentRect().width - width
                   : root.runDockEdge === "left" ? root.mainContentRect().x
                   : root.mainContentRect().x + (root.mainContentRect().width - width) / 2
                y: root.runDockEdge === "bottom" ? root.mainContentRect().y + root.mainContentRect().height - height
                   : root.runDockEdge === "top" ? root.mainContentRect().y
                   : root.mainContentRect().y + (root.mainContentRect().height - height) / 2
                color: "#F4F4F4"
                border.color: "#000000"
                Text { anchors.centerIn: parent; text: root.runWindowTitle; color: "#000000"; rotation: root.runDockEdge === "left" || root.runDockEdge === "right" ? -90 : 0 }
                MouseArea { anchors.fill: parent; onClicked: {} }
            }

            Rectangle {
                id: runWindow
                visible: false
                x: root.runWindowEmbedded ? root.dockedRunX() : root.runFloatX
                y: root.runWindowEmbedded ? root.dockedRunY() : root.runFloatY
                width: root.runWindowEmbedded ? root.dockedRunWidth() : root.runFloatWidth
                height: root.runWindowEmbedded ? root.dockedRunHeight() : root.runFloatHeight
                color: "#FFFFFF"
                border.color: "#000000"
                clip: true

                Component { id: positionRunPanelComponent; PositionRunPanel { anchors.fill: parent } }
                Component { id: jogRunPanelComponent; JogRunPanel { anchors.fill: parent } }

                Rectangle {
                    id: runTitleBar
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    height: 28
                    color: "#D9F2F0"
                    border.color: "#7DAAA6"
                    property real dragStartX: 0
                    property real dragStartY: 0

                    Text { anchors.left: parent.left; anchors.leftMargin: 8; anchors.verticalCenter: parent.verticalCenter; text: root.runWindowTitle; color: "#000000"; font.weight: root.appEmphasisFontWeight }
                    Row {
                        anchors.right: parent.right
                        anchors.rightMargin: 4
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 4
                        Repeater {
                            model: [
                                { "label": "\u6d6e", "action": "float" },
                                { "label": "\u4e0a", "action": "top" },
                                { "label": "\u4e0b", "action": "bottom" },
                                { "label": "\u5de6", "action": "left" },
                                { "label": "\u53f3", "action": "right" },
                                { "label": "\u00d7", "action": "close" }
                            ]
                            delegate: Rectangle {
                                required property var modelData
                                width: 24
                                height: 20
                                color: actionMouse.pressed ? "#D4E8C5" : (actionMouse.containsMouse ? "#E5F2D8" : "#FFFFFF")
                                border.color: "#9E9E9E"
                                Text { anchors.centerIn: parent; text: modelData.label; color: "#000000"; font.pixelSize: 11 }
                                MouseArea {
                                    id: actionMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    onClicked: {
                                        if (modelData.action === "float") {
                                            root.floatRunWindow()
                                        } else if (modelData.action === "top" || modelData.action === "bottom" || modelData.action === "left" || modelData.action === "right") {
                                            root.dockRunWindow(modelData.action)
                                        } else {
                                            root.runWindowVisible = false
                                            root.runWindowVisible = false
                                        }
                                    }
                                }
                            }
                        }
                    }
                    DragHandler {
                        id: runDrag
                        target: null
                        onActiveChanged: {
                            if (active) {
                                runTitleBar.dragStartX = runWindow.x
                                runTitleBar.dragStartY = runWindow.y
                                if (root.runWindowEmbedded) {
                                    root.runFloatX = runWindow.x
                                    root.runFloatY = runWindow.y
                                    root.runFloatWidth = runWindow.width
                                    root.runFloatHeight = runWindow.height
                                    root.runWindowEmbedded = false
                                }
                            } else {
                                var dropPoint = runTitleBar.mapToItem(appSurface, runDrag.centroid.position.x, runDrag.centroid.position.y)
                                root.finishRunWindowDrag(dropPoint.x, dropPoint.y)
                            }
                        }
                        onTranslationChanged: {
                            if (active && !root.runWindowEmbedded) {
                                root.runFloatX = Math.max(0, Math.min(runTitleBar.dragStartX + translation.x, appSurface.width - runWindow.width))
                                root.runFloatY = Math.max(0, Math.min(runTitleBar.dragStartY + translation.y, appSurface.height - runWindow.height))
                            }
                        }
                    }
                }

                Loader {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: runTitleBar.bottom
                    anchors.bottom: parent.bottom
                    anchors.margins: 2
                    sourceComponent: root.runWindowMode === "position" ? positionRunPanelComponent : jogRunPanelComponent
                }

                Rectangle {
                    id: dockResizeHandleH
                    visible: false
                    width: 4
                    height: parent.height
                    x: root.runDockEdge === "right" ? 0 : parent.width - width
                    color: dockResizeHoverH.hovered || dockResizeDragH.active ? "#7DAAA6" : "#B8B8B8"
                    property real startFraction: 0
                    HoverHandler { id: dockResizeHoverH; cursorShape: Qt.SplitHCursor }
                    DragHandler {
                        id: dockResizeDragH
                        target: null
                        onActiveChanged: if (active) dockResizeHandleH.startFraction = root.runDockFraction
                        onTranslationChanged: {
                            if (!active) return
                            var area = root.mainContentRect()
                            var delta = translation.x / Math.max(1, area.width)
                            root.runDockFraction = Math.max(0.25, Math.min(0.8, dockResizeHandleH.startFraction + (root.runDockEdge === "right" ? -delta : delta)))
                        }
                    }
                }

                Rectangle {
                    id: dockResizeHandleV
                    visible: false
                    width: parent.width
                    height: 4
                    y: root.runDockEdge === "bottom" ? 0 : parent.height - height
                    color: dockResizeHoverV.hovered || dockResizeDragV.active ? "#7DAAA6" : "#B8B8B8"
                    property real startFraction: 0
                    HoverHandler { id: dockResizeHoverV; cursorShape: Qt.SplitVCursor }
                    DragHandler {
                        id: dockResizeDragV
                        target: null
                        onActiveChanged: if (active) dockResizeHandleV.startFraction = root.runDockFraction
                        onTranslationChanged: {
                            if (!active) return
                            var area = root.mainContentRect()
                            var delta = translation.y / Math.max(1, area.height)
                            root.runDockFraction = Math.max(0.25, Math.min(0.8, dockResizeHandleV.startFraction + (root.runDockEdge === "bottom" ? -delta : delta)))
                        }
                    }
                }

                Rectangle {
                    id: floatResizeHandle
                    visible: !root.runWindowEmbedded
                    width: 14
                    height: 14
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    color: resizeFloatHover.hovered || floatResizeDrag.active ? "#7DAAA6" : "#D0D0D0"
                    property real startWidth: 0
                    property real startHeight: 0
                    HoverHandler { id: resizeFloatHover; cursorShape: Qt.SizeFDiagCursor }
                    DragHandler {
                        id: floatResizeDrag
                        target: null
                        onActiveChanged: {
                            if (active) {
                                floatResizeHandle.startWidth = root.runFloatWidth
                                floatResizeHandle.startHeight = root.runFloatHeight
                            }
                        }
                        onTranslationChanged: {
                            if (!active) return
                            root.runFloatWidth = Math.max(300, floatResizeHandle.startWidth + translation.x)
                            root.runFloatHeight = Math.max(210, floatResizeHandle.startHeight + translation.y)
                        }
                    }
                }
            }
        }
    }

    RunToolWindow {
        id: runToolWindow
        visible: root.runWindowVisible && !root.runWindowEmbedded
        x: root.runFloatX
        y: root.runFloatY
        width: root.runFloatWidth
        height: root.runFloatHeight
        mode: root.runWindowMode
        titleText: root.runWindowTitle

        onMoveRequested: function(nextX, nextY, screenX, screenY) {
            root.runFloatX = nextX
            root.runFloatY = nextY
            root.updateRunDockPreview(screenX, screenY)
        }
        onDragReleased: function(screenX, screenY) {
            root.finishRunWindowDrag(screenX, screenY)
        }
        onResizeRequested: function(nextWidth, nextHeight) {
            root.runFloatWidth = nextWidth
            root.runFloatHeight = nextHeight
        }
        onVisibleChanged: {
            if (visible) {
                requestActivate()
            }
        }
    }

    Dialog {
        id: communicationDialog
        title: qsTr("通讯设置")
        modal: true
        width: 380
        x: (root.width - width) / 2
        y: (root.height - height) / 2
        standardButtons: Dialog.Ok | Dialog.Cancel
        background: Rectangle { color: "#FFFFFF"; border.color: "#A8A8A8" }

        onOpened: {
            appController.refreshSerialPorts()
            portCombo.currentIndex = Math.max(0, portCombo.find(appController.portName))
            addressSpin.value = appController.slaveAddress
            baudCombo.currentIndex = Math.max(0, baudCombo.find(String(appController.baudRate)))
            formatCombo.currentIndex = Math.max(0, formatCombo.find(appController.serialFormat))
            timeoutSpin.value = appController.responseTimeoutMs
            retrySpin.value = appController.retryCount
        }

        onAccepted: {
            appController.setCommunicationSettings(portCombo.editable ? portCombo.editText : portCombo.currentText,
                                                   addressSpin.value,
                                                   Number(baudCombo.editable ? baudCombo.editText : baudCombo.currentText),
                                                   formatCombo.currentText,
                                                   timeoutSpin.value,
                                                   retrySpin.value)
        }

        contentItem: ColumnLayout {
            spacing: 10

            Label { text: qsTr("串口"); color: "#000000" }
            AppComboBox { id: portCombo; Layout.fillWidth: true; editable: true; model: appController.serialPortNames }

            Label { text: qsTr("Modbus 地址"); color: "#000000" }
            AppSpinBox { id: addressSpin; Layout.fillWidth: true; from: 1; to: 247; value: 1 }

            Label { text: qsTr("波特率"); color: "#000000" }
            AppComboBox { id: baudCombo; Layout.fillWidth: true; editable: true; model: ["9600", "19200", "38400", "57600", "115200", "230400", "460800", "921600"] }

            Label { text: qsTr("通信格式"); color: "#000000" }
            AppComboBox { id: formatCombo; Layout.fillWidth: true; model: appController.communicationFormats }

            Label { text: qsTr("响应超时 ms"); color: "#000000" }
            AppSpinBox { id: timeoutSpin; Layout.fillWidth: true; from: 100; to: 10000; stepSize: 100; value: 1000 }

            Label { text: qsTr("重试次数"); color: "#000000" }
            AppSpinBox { id: retrySpin; Layout.fillWidth: true; from: 0; to: 10; value: 3 }
        }
    }

    Popup {
        id: toast
        modal: false
        focus: false
        closePolicy: Popup.CloseOnPressOutside | Popup.CloseOnEscape
        x: (root.width - width) / 2
        y: 70
        padding: 12
        background: Rectangle { color: "#FFFFFF"; border.color: "#A8A8A8" }

        function show(message) {
            toastLabel.text = message
            open()
            toastTimer.restart()
        }

        contentItem: Label {
            id: toastLabel
            color: "#000000"
        }

        Timer {
            id: toastTimer
            interval: 2000
            repeat: false
            onTriggered: toast.close()
        }
    }
}
