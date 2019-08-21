import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.3
import QtQuick.Controls.Material 2.12
import thumper 1.0

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
