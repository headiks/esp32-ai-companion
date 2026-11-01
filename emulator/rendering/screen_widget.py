"""Round 466x466 screen: blits the framebuffer rendered by the core and turns the mouse into touch."""
import numpy as np
from PySide6.QtCore import QPointF, QRectF, Qt
from PySide6.QtGui import QBrush, QColor, QImage, QPainter, QPainterPath, QPen
from PySide6.QtWidgets import QWidget

SCREEN = 466
BEZEL = 26


class ScreenWidget(QWidget):
    def __init__(self, on_touch):
        super().__init__()
        self.on_touch = on_touch
        self.fb = np.zeros((SCREEN, SCREEN), dtype=np.uint16)
        self.setFixedSize(SCREEN + 2 * BEZEL, SCREEN + 2 * BEZEL)
        self.setCursor(Qt.PointingHandCursor)
        self.setMouseTracking(False)
        path = QPainterPath()
        path.addEllipse(QRectF(BEZEL, BEZEL, SCREEN, SCREEN))
        self.clip = path

    def show_frame(self, fb):
        self.fb = fb
        self.update()

    def paintEvent(self, _):
        p = QPainter(self)
        p.setRenderHint(QPainter.Antialiasing)
        p.setRenderHint(QPainter.SmoothPixmapTransform)  # keeps the framebuffer smooth on scaled desktops
        p.fillRect(self.rect(), QColor(24, 24, 28))
        # Metallic housing as on the real device.
        p.setPen(QPen(QColor(150, 152, 158), 3))
        p.setBrush(QBrush(QColor(60, 62, 68)))
        p.drawEllipse(QRectF(5, 5, SCREEN + 2 * BEZEL - 10, SCREEN + 2 * BEZEL - 10))
        p.setClipPath(self.clip)
        img = QImage(self.fb.data, SCREEN, SCREEN, SCREEN * 2, QImage.Format_RGB16)
        p.drawImage(BEZEL, BEZEL, img)
        p.end()

    def _pos(self, e):
        pt = e.position() if hasattr(e, "position") else QPointF(e.x(), e.y())
        return max(0, min(SCREEN - 1, int(pt.x()) - BEZEL)), max(0, min(SCREEN - 1, int(pt.y()) - BEZEL))

    def mousePressEvent(self, e):
        if e.button() == Qt.LeftButton:
            self.on_touch(True, *self._pos(e))

    def mouseMoveEvent(self, e):
        if e.buttons() & Qt.LeftButton:
            self.on_touch(True, *self._pos(e))

    def mouseReleaseEvent(self, e):
        if e.button() == Qt.LeftButton:
            self.on_touch(False, *self._pos(e))
