import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Controls.Material
import thumper

Button {
  id: root
  property color backgroundColor: 'green'
  property string tag
  property int count

  text: "%1 (%2)".arg(tag).arg(count)
  focusPolicy: Qt.NoFocus

  font.capitalization: Font.MixedCase

  background: Rectangle {
    implicitWidth: 64
    implicitHeight: root.Material.buttonHeight
    id: bgRect
    radius: 4
    color: (root.visualFocus || root.hovered || root.highlighted) ? Qt.lighter(backgroundColor) : root.backgroundColor
  }
}
