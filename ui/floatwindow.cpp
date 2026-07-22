#include "floatwindow.h"
#include "docmind/DocumentEngine.h"
#include "docmind/core/ConfigManager.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QPainter>
#include <QPainterPath>
#include <QClipboard>
#include <QApplication>
#include <QIcon>
#include <QCloseEvent>
#include <QMouseEvent>

FloatTranslateWindow::FloatTranslateWindow(QWidget* parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint);
    setMinimumSize(300, 140);
    setMaximumSize(600, 400);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    setAttribute(Qt::WA_TranslucentBackground);

    // ============================================================
    // Title bar (36px) — teal
    // ============================================================
    QWidget* titleBar = new QWidget(this);
    titleBar->setFixedHeight(36);

    auto* titleIcon = new QLabel;
    titleIcon->setPixmap(QIcon(":/icons/selection.png").pixmap(18, 18));
    titleIcon->setFixedSize(18, 18);

    auto* titleLabel = new QLabel(tr("Selection Translation"));
    titleLabel->setStyleSheet("color: white; font-weight: bold; font-size: 13px;");

    lockBtn_ = new QPushButton;
    lockBtn_->setFixedSize(28, 28);
    lockBtn_->setCursor(Qt::PointingHandCursor);
    lockBtn_->setStyleSheet(
        "QPushButton { background: transparent; border: none; border-radius: 14px; }"
        "QPushButton:hover { background: rgba(255,255,255,0.2); }");
    lockBtn_->setIcon(QIcon(":/icons/unLock.png"));
    lockBtn_->setIconSize(QSize(22, 22));
    connect(lockBtn_, &QPushButton::clicked, this, &FloatTranslateWindow::onToggleLock);

    auto* closeBtn = new QPushButton(this);
    closeBtn->setFixedSize(28, 28);
    closeBtn->setIcon(QIcon(":/icons/close.png"));
    closeBtn->setIconSize(QSize(18, 18));
    closeBtn->setCursor(Qt::PointingHandCursor);
    closeBtn->setStyleSheet(
        "QPushButton { background: transparent; border: none; border-radius: 14px; }"
        "QPushButton:hover { background: rgba(255,255,255,0.2); }");
    connect(closeBtn, &QPushButton::clicked, this, &QWidget::hide);

    QHBoxLayout* titleLayout = new QHBoxLayout(titleBar);
    titleLayout->setContentsMargins(12, 0, 4, 0);
    titleLayout->setSpacing(6);
    titleLayout->addWidget(titleIcon);
    titleLayout->addWidget(titleLabel);
    titleLayout->addStretch();
    titleLayout->addWidget(lockBtn_);
    titleLayout->addWidget(closeBtn);

    // ============================================================
    // Content area
    // ============================================================
    // Source text (read-only, auto-height based on content)
    sourceText_ = new QTextEdit(this);
    sourceText_->setReadOnly(true);
    sourceText_->setMaximumHeight(50);
    sourceText_->setStyleSheet(
        "QTextEdit { border: none; padding: 4px 0; font-size: 12px; color: #6B7280; background: transparent; }");
    sourceText_->setPlaceholderText(tr("Selected text will appear here…"));

    // Separator
    QFrame* separator = new QFrame(this);
    separator->setFixedHeight(1);
    separator->setStyleSheet("background-color: #E5E5EA; border: none;");

    // Result text (read-only, elastic)
    resultText_ = new QTextEdit(this);
    resultText_->setReadOnly(true);
    resultText_->setMinimumHeight(30);
    resultText_->setStyleSheet(
        "QTextEdit { border: none; padding: 6px 0; font-size: 13px; color: #1D1D1F; background: transparent; }");
    resultText_->setPlaceholderText(tr("Translation result will appear here…"));

    // ============================================================
    // Bottom bar
    // ============================================================
    langCombo_ = new QComboBox(this);
    langCombo_->setEditable(true);
    langCombo_->setInsertPolicy(QComboBox::NoInsert);
    langCombo_->addItems({
        tr("Chinese"),
        tr("English"),
        tr("French"),
        tr("Portuguese"),
        tr("Spanish"),
        tr("Japanese"),
        tr("Turkish"),
        tr("Russian"),
        tr("Arabic"),
        tr("Korean"),
        tr("Thai"),
        tr("Italian"),
        tr("German"),
        tr("Vietnamese"),
        tr("Malay"),
        tr("Indonesian"),
        tr("Filipino"),
        tr("Hindi"),
        tr("Traditional Chinese"),
        tr("Polish"),
        tr("Czech"),
        tr("Dutch"),
        tr("Khmer"),
        tr("Burmese"),
        tr("Persian"),
        tr("Gujarati"),
        tr("Urdu"),
        tr("Telugu"),
        tr("Marathi"),
        tr("Hebrew"),
        tr("Bengali"),
        tr("Tamil"),
        tr("Ukrainian"),
        tr("Tibetan"),
        tr("Kazakh"),
        tr("Mongolian"),
        tr("Uyghur"),
        tr("Cantonese"),
    });
    // 从 ConfigManager 读取用户设置的默认语言
    {
        auto& cfg = docmind::ConfigManager::getInstance();
        langCombo_->setCurrentText(QString::fromStdString(cfg.getDefaultLanguage()));
    }
    langCombo_->setFixedHeight(30);
    langCombo_->setMinimumWidth(110);
    langCombo_->setStyleSheet(
        "QComboBox { border: 1px solid #D1D1D6; border-radius: 6px; padding: 4px 8px; font-size: 12px; background: white; }");

    translateBtn_ = new QPushButton(tr("Translate"));
    translateBtn_->setIconSize(QSize(16, 16));
    translateBtn_->setFixedHeight(30);
    translateBtn_->setObjectName("primaryBtn");

    copyBtn_ = new QPushButton(tr("Copy"));
    copyBtn_->setFixedHeight(30);
    copyBtn_->setObjectName("secondaryBtn");

    connect(translateBtn_, &QPushButton::clicked, this, &FloatTranslateWindow::onTranslate);
    connect(copyBtn_, &QPushButton::clicked, this, &FloatTranslateWindow::onCopyResult);

    QHBoxLayout* bottomLayout = new QHBoxLayout;
    bottomLayout->setContentsMargins(0, 6, 0, 0);
    bottomLayout->setSpacing(8);
    bottomLayout->addWidget(langCombo_);
    bottomLayout->addWidget(translateBtn_);
    bottomLayout->addWidget(copyBtn_);
    bottomLayout->addStretch();

    // ============================================================
    // Content layout
    // ============================================================
    QVBoxLayout* contentLayout = new QVBoxLayout;
    contentLayout->setContentsMargins(12, 6, 12, 8);
    contentLayout->setSpacing(2);
    contentLayout->addWidget(sourceText_);
    contentLayout->addWidget(separator);
    contentLayout->addWidget(resultText_, 1);
    contentLayout->addLayout(bottomLayout);

    // ============================================================
    // Main layout
    // ============================================================
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    mainLayout->addWidget(titleBar);
    mainLayout->addLayout(contentLayout);
}

FloatTranslateWindow::~FloatTranslateWindow() {}

void FloatTranslateWindow::setStayOnTop(bool on) {
    if (on)
        setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);
    else
        setWindowFlags(windowFlags() & ~Qt::WindowStaysOnTopHint);
    show();
}

void FloatTranslateWindow::translateText(const QString& text) {
    performTranslate(text);
}

// ============================================================
// paintEvent — rounded white background with teal title bar
// ============================================================
void FloatTranslateWindow::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QPainterPath path;
    path.addRoundedRect(rect(), 8, 8);
    painter.setClipPath(path);

    // White main background
    painter.fillRect(rect(), Qt::white);

    // Teal title bar (top 36px)
    painter.fillRect(0, 0, width(), 36, QColor("#0B7C72"));
}

// ============================================================
// Mouse events — title bar dragging
// ============================================================
void FloatTranslateWindow::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && event->position().y() <= 36 && !locked_) {
        dragging_ = true;
        dragStartPos_ = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
    }
}

void FloatTranslateWindow::mouseMoveEvent(QMouseEvent* event) {
    if (dragging_ && (event->buttons() & Qt::LeftButton) && !locked_) {
        move(event->globalPosition().toPoint() - dragStartPos_);
        event->accept();
    }
}

void FloatTranslateWindow::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        dragging_ = false;
        event->accept();
    }
}

// ============================================================
// closeEvent — hide instead of close
// ============================================================
void FloatTranslateWindow::closeEvent(QCloseEvent* event) {
    hide();
    event->ignore();
}

// ============================================================
// Private slots
// ============================================================
void FloatTranslateWindow::onTranslate() {
    if (!sourceText_->toPlainText().isEmpty()) {
        performTranslate(sourceText_->toPlainText());
    }
}

void FloatTranslateWindow::onCopyResult() {
    QApplication::clipboard()->setText(resultText_->toPlainText());
}

void FloatTranslateWindow::onToggleLock() {
    locked_ = !locked_;
    lockBtn_->setIcon(QIcon(locked_ ? QStringLiteral(":/icons/Lock.png") : QStringLiteral(":/icons/unLock.png")));
    lockBtn_->setIconSize(QSize(22, 22));
    setStayOnTop(locked_);
}

// ============================================================
// Private methods
// ============================================================
void FloatTranslateWindow::performTranslate(const QString& text) {
    if (text.isEmpty()) return;

    sourceText_->setText(text);

    std::string result = translate_text(
        text.toStdString(),
        langCombo_->currentText().toStdString()
    );

    resultText_->setText(QString::fromStdString(result));
    emit translationDone(text, QString::fromStdString(result));

    // 自适应高度：根据内容高度调整窗口
    adjustSize();
    // 根据内容调整窗口大小
    int docH = (int)resultText_->document()->size().height();
    resultText_->setFixedHeight(qBound(50, docH + 20, 300));
    adjustSize();
    int srcW = (int)sourceText_->document()->size().width();
    int resW = (int)resultText_->document()->size().width();
    int winW = qMin(600, qMax(300, qMax(srcW, resW) + 60));
    resize(winW, sizeHint().height());

    show();
    raise();
    activateWindow();
}
