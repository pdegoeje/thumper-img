import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.3
import thumper 1.0

Button {
  id: root
  property color backgroundColor: 'green'

  font.capitalization: Font.MixedCase

  background: Rectangle {
    id: bgRect
    radius: 4
    color: root.visualFocus || root.hovered ? Qt.lighter(backgroundColor) : root.backgroundColor
  }
}
