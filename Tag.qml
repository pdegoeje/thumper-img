import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.3
import thumper 1.0

Label {
  id: root
  signal clicked()
  property bool active: true
  property alias backgroundColor: bgRect.color

  color: active ? 'white' : 'black'
  backgroundColor: active ? 'green' : 'grey'

  padding: 4
  
  background: Rectangle {
    id: bgRect
    radius: 4
  }
  
  MouseArea {
    anchors.fill: parent
    
    onClicked: root.clicked()
  }
}
