#include "aerosim/ui/postprocess/PostProcessWorkbenchWindow.h"

#include <QAction>
#include <QAbstractItemView>
#include <QCheckBox>
#include <QComboBox>
#include <QDockWidget>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenuBar>
#include <QPushButton>
#include <QSlider>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTextEdit>
#include <QTabWidget>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

namespace aerosim {
namespace {

template <size_t N>
QString uiText(const char16_t (&characters)[N])
{
    return QString::fromUtf16(reinterpret_cast<const ushort *>(characters), int(N) - 1);
}

QToolButton *addToolButton(QToolBar *toolbar, const QString &text, const QString &toolTip)
{
    auto *button = new QToolButton(toolbar);
    button->setText(text);
    button->setToolTip(toolTip);
    button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    toolbar->addWidget(button);
    return button;
}

QLabel *sectionLabel(const QString &text, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setObjectName(uiText(u"sectionTitle"));
    return label;
}

} // namespace

PostProcessWorkbenchWindow::PostProcessWorkbenchWindow(QWidget *parent) : QMainWindow(parent)
{
    setObjectName(uiText(u"postProcessWorkbench"));
    setWindowTitle(uiText(u"AeroSimWorkbench - \u540e\u5904\u7406\u5de5\u4f5c\u53f0"));
    resize(1200, 760);
    setMinimumSize(1120, 680);
    createActions();

    auto *workbench = new QWidget(this);
    auto *rootLayout = new QVBoxLayout(workbench);
    rootLayout->setContentsMargins(6, 6, 6, 0);
    rootLayout->setSpacing(6);

    auto *content = new QSplitter(Qt::Horizontal, workbench);
    content->setObjectName(uiText(u"postProcessContent"));
    auto *leftTabs = new QTabWidget(content);
    leftTabs->setObjectName(uiText(u"postProcessNavigator"));
    leftTabs->setMinimumWidth(260);
    leftTabs->setMaximumWidth(340);
    leftTabs->addTab(createDataPanel(), uiText(u"\u6570\u636e"));
    leftTabs->addTab(createScenePanel(), uiText(u"\u573a\u666f"));
    content->addWidget(leftTabs);
    content->addWidget(createViewportPanel());
    content->addWidget(createPropertiesPanel());
    content->setStretchFactor(0, 0);
    content->setStretchFactor(1, 1);
    content->setStretchFactor(2, 0);
    content->setChildrenCollapsible(false);
    content->setCollapsible(0, false);
    content->setCollapsible(2, false);
    content->setSizes({260, 850, 300});
    QTimer::singleShot(0, content, [content] { content->setSizes({280, 840, 320}); });
    rootLayout->addWidget(content, 1);
    rootLayout->addWidget(createTimeline());
    setCentralWidget(workbench);

    auto *messageDock = new QDockWidget(uiText(u"\u540e\u5904\u7406\u6d88\u606f"), this);
    messageDock->setObjectName(uiText(u"postProcessMessageDock"));
    events_ = new QTextEdit(messageDock);
    events_->setObjectName(uiText(u"postProcessEvents"));
    events_->setReadOnly(true);
    events_->setMinimumHeight(100);
    messageDock->setWidget(events_);
    addDockWidget(Qt::BottomDockWidgetArea, messageDock);
    messageDock->hide();

    statusBar()->showMessage(uiText(u"\u540e\u5904\u7406\u5de5\u4f5c\u53f0\u5df2\u5c31\u7eea\uff1a\u7b49\u5f85\u7ed3\u679c\u6570\u636e\u4e0e VTK \u573a\u666f\u670d\u52a1\u63a5\u5165"));
    setStyleSheet(uiText(u""
        "QMainWindow { background: #edf1f5; }"
        "QToolBar { background: #ffffff; border-bottom: 1px solid #cbd4df; spacing: 8px; padding: 4px; }"
        "QToolButton { min-width: 52px; min-height: 40px; color: #233246; }"
        "QToolButton:hover { background: #e4eef9; border-radius: 4px; }"
        "QTabWidget::pane, QFrame#viewportFrame, QGroupBox { background: #ffffff; border: 1px solid #cbd4df; }"
        "QTabBar::tab { padding: 8px 18px; background: #e6ebf1; }"
        "QTabBar::tab:selected { background: #ffffff; color: #0b5ca8; }"
        "QListWidget { border: 0; background: #ffffff; outline: 0; }"
        "QListWidget::item { min-height: 34px; padding: 5px 8px; border-bottom: 1px solid #edf1f5; }"
        "QListWidget::item:selected { background: #e4eef9; color: #0b5ca8; }"
        "QFrame#dataCard { background: #f5f8fc; border: 1px solid #d9e3ee; border-radius: 5px; }"
        "QPushButton#primaryAction { background: #0b69b7; color: white; border: 0; border-radius: 4px; padding: 5px 10px; }"
        "QLabel#mutedText { color: #6e7f91; font-size: 12px; }"
        "QLabel#sectionTitle { color: #31445d; font-weight: bold; padding: 5px 2px; }"
        "QGroupBox { margin-top: 12px; padding: 8px; font-weight: bold; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 4px; }"
        "QTextEdit { background: #182432; color: #cfe2f5; border: 0; }"));
    activateMockEntry(0);
}

void PostProcessWorkbenchWindow::createActions()
{
    auto *fileMenu = menuBar()->addMenu(uiText(u"\u6587\u4ef6"));
    auto *importAction = fileMenu->addAction(uiText(u"\u5bfc\u5165\u7ed3\u679c\u2026"));
    auto *exportAction = fileMenu->addAction(uiText(u"\u5bfc\u51fa\u5f53\u524d\u5bf9\u8c61\u2026"));
    auto *viewMenu = menuBar()->addMenu(uiText(u"\u89c6\u56fe"));
    auto *fitAction = viewMenu->addAction(uiText(u"\u9002\u914d\u89c6\u56fe"));
    auto *featureMenu = menuBar()->addMenu(uiText(u"\u7279\u5f81"));
    auto *sliceAction = featureMenu->addAction(uiText(u"\u521b\u5efa\u5207\u9762"));
    auto *isoAction = featureMenu->addAction(uiText(u"\u521b\u5efa\u7b49\u503c\u9762"));
    auto *streamAction = featureMenu->addAction(uiText(u"\u521b\u5efa\u6d41\u7ebf"));
    auto *analysisMenu = menuBar()->addMenu(uiText(u"\u5206\u6790"));
    auto *curveAction = analysisMenu->addAction(uiText(u"\u6253\u5f00\u66f2\u7ebf\u5206\u6790"));
    auto *toolsMenu = menuBar()->addMenu(uiText(u"\u5de5\u5177"));
    auto *deepSeekAction = toolsMenu->addAction(uiText(u"DeepSeek Harness\u2026"));

    auto *toolbar = addToolBar(uiText(u"\u540e\u5904\u7406"));
    toolbar->setObjectName(uiText(u"postProcessToolbar"));
    auto *importButton = addToolButton(toolbar, uiText(u"\u5bfc\u5165\u7ed3\u679c"), uiText(u"\u5bfc\u5165\u4e00\u4e2a\u6216\u591a\u4e2a\u7ed3\u679c\u6587\u4ef6"));
    auto *meshButton = addToolButton(toolbar, uiText(u"\u7f51\u683c"), uiText(u"\u5207\u6362\u7f51\u683c\u548c\u8fb9\u7ebf\u663e\u793a"));
    auto *contourButton = addToolButton(toolbar, uiText(u"\u4e91\u56fe"), uiText(u"\u9009\u62e9\u573a\u53d8\u91cf\u5e76\u663e\u793a\u4e91\u56fe"));
    auto *sliceButton = addToolButton(toolbar, uiText(u"\u5207\u9762"), uiText(u"\u521b\u5efa\u72ec\u7acb\u7684\u5207\u9762\u5bf9\u8c61"));
    auto *isoButton = addToolButton(toolbar, uiText(u"\u7b49\u503c\u9762"), uiText(u"\u521b\u5efa\u4e09\u7ef4\u7b49\u503c\u9762"));
    auto *streamButton = addToolButton(toolbar, uiText(u"\u6d41\u7ebf"), uiText(u"\u8bbe\u7f6e\u79cd\u5b50\u5e76\u521b\u5efa\u6d41\u7ebf"));
    auto *fitButton = addToolButton(toolbar, uiText(u"\u9002\u914d\u89c6\u56fe"), uiText(u"\u4f7f\u573a\u666f\u9002\u914d\u5f53\u524d\u89c6\u53e3"));
    auto *exportButton = addToolButton(toolbar, uiText(u"\u5bfc\u51fa"), uiText(u"\u5bfc\u51fa\u7279\u5f81\u3001\u56fe\u50cf\u6216\u52a8\u753b"));
    toolbar->addSeparator();
    auto *deepSeekButton = addToolButton(toolbar, uiText(u"DeepSeek\nHarness"), uiText(u"\u6253\u5f00 DeepSeek Harness \u52a9\u624b\u5165\u53e3"));
    featureButtons_ = {meshButton, contourButton, sliceButton, isoButton, streamButton};
    meshButton->setObjectName(uiText(u"postProcessMeshButton"));
    contourButton->setObjectName(uiText(u"postProcessContourButton"));
    sliceButton->setObjectName(uiText(u"postProcessSliceButton"));
    isoButton->setObjectName(uiText(u"postProcessIsoButton"));
    streamButton->setObjectName(uiText(u"postProcessStreamButton"));

    auto command = [this](const QString &name, const QString &message) {
        emit postProcessCommandRequested(name);
        appendMessage(message);
    };
    connect(importAction, &QAction::triggered, this, [command] { command(uiText(u"importResults"), uiText(u"\u5bfc\u5165\u7ed3\u679c\u8bf7\u6c42\u5df2\u63d0\u4ea4\uff1b\u8bfb\u53d6\u670d\u52a1\u63a5\u5165\u540e\u5c06\u5728\u5de5\u4f5c\u7ebf\u7a0b\u52a0\u8f7d\u3002")); });
    connect(exportAction, &QAction::triggered, this, [command] { command(uiText(u"exportObject"), uiText(u"\u5bfc\u51fa\u8bf7\u6c42\u5df2\u63d0\u4ea4\uff1b\u7b49\u5f85\u9009\u4e2d\u7684\u573a\u666f\u5bf9\u8c61\u3002")); });
    connect(fitAction, &QAction::triggered, this, [this] { emit viewRequested(uiText(u"reset")); appendMessage(uiText(u"\u89c6\u56fe\u5df2\u91cd\u7f6e\u3002")); });
    connect(sliceAction, &QAction::triggered, this, [command] { command(uiText(u"createSlice"), uiText(u"\u521b\u5efa\u5207\u9762\u8bf7\u6c42\u5df2\u63d0\u4ea4\u3002")); });
    connect(isoAction, &QAction::triggered, this, [command] { command(uiText(u"createIsoSurface"), uiText(u"\u521b\u5efa\u7b49\u503c\u9762\u8bf7\u6c42\u5df2\u63d0\u4ea4\u3002")); });
    connect(streamAction, &QAction::triggered, this, [command] { command(uiText(u"createStreamline"), uiText(u"\u521b\u5efa\u6d41\u7ebf\u8bf7\u6c42\u5df2\u63d0\u4ea4\u3002")); });
    connect(curveAction, &QAction::triggered, this, [command] { command(uiText(u"openCurveAnalysis"), uiText(u"\u66f2\u7ebf\u5206\u6790\u5de5\u4f5c\u533a\u8bf7\u6c42\u5df2\u63d0\u4ea4\u3002")); });
    auto openDeepSeekHarness = [this] {
        if (!deepSeekHarnessDock_) {
            deepSeekHarnessDock_ = new QDockWidget(uiText(u"DeepSeek Harness"), this);
            deepSeekHarnessDock_->setObjectName(uiText(u"deepSeekHarnessDock"));
            deepSeekHarnessDock_->setMinimumWidth(400);
            deepSeekHarnessDock_->resize(460, 620);
            auto *panel = new QWidget(deepSeekHarnessDock_);
            auto *layout = new QVBoxLayout(panel);
            auto *description = new QLabel(uiText(u"\u9488\u5bf9\u5f53\u524d\u7ed3\u679c\u3001\u9009\u4e2d\u56fe\u5c42\u6216\u540e\u5904\u7406\u6d41\u7a0b\u63d0\u4ea4\u4efb\u52a1\u3002"), panel);
            description->setWordWrap(true);
            layout->addWidget(description);
            auto *scope = new QComboBox(panel);
            scope->addItems({uiText(u"\u5f53\u524d\u89c6\u53e3"), uiText(u"\u9009\u4e2d\u56fe\u5c42"), uiText(u"\u5f53\u524d\u6570\u636e\u96c6")});
            layout->addWidget(scope);
            auto *prompt = new QTextEdit(panel);
            prompt->setObjectName(uiText(u"deepSeekHarnessPrompt"));
            prompt->setPlaceholderText(uiText(u"\u4f8b\u5982\uff1a\u751f\u6210\u538b\u529b\u4e91\u56fe\u5e76\u63d0\u53d6 0.8 \u7b49\u503c\u9762\u3002"));
            prompt->setMinimumHeight(220);
            layout->addWidget(prompt);
            auto *run = new QPushButton(uiText(u"\u8fd0\u884c Harness"), panel);
            run->setObjectName(uiText(u"primaryAction"));
            layout->addWidget(run);
            auto *state = new QLabel(uiText(u"\u5c1a\u672a\u8fde\u63a5\u6a21\u578b\u3002UI \u5165\u53e3\u5df2\u5c31\u7eea\u3002"), panel);
            state->setObjectName(uiText(u"mutedText"));
            state->setWordWrap(true);
            layout->addWidget(state);
            layout->addStretch();
            deepSeekHarnessDock_->setWidget(panel);
            addDockWidget(Qt::RightDockWidgetArea, deepSeekHarnessDock_);
            connect(run, &QPushButton::clicked, this, [this, prompt] {
                emit postProcessCommandRequested(uiText(u"openDeepSeekHarness"));
                appendMessage(prompt->toPlainText().isEmpty()
                    ? uiText(u"DeepSeek Harness \u5df2\u6253\u5f00\uff1a\u8bf7\u8f93\u5165\u540e\u5904\u7406\u4efb\u52a1\u3002")
                    : uiText(u"DeepSeek Harness \u4efb\u52a1\u5df2\u63d0\u4ea4\uff1b\u7b49\u5f85 Harness \u670d\u52a1\u63a5\u5165\u3002"));
            });
        }
        deepSeekHarnessDock_->show();
        deepSeekHarnessDock_->raise();
        emit postProcessCommandRequested(uiText(u"openDeepSeekHarness"));
        appendMessage(uiText(u"DeepSeek Harness \u5165\u53e3\u5df2\u6253\u5f00\u3002"));
    };
    connect(deepSeekAction, &QAction::triggered, this, openDeepSeekHarness);
    connect(importButton, &QToolButton::clicked, importAction, &QAction::trigger);
    connect(meshButton, &QToolButton::clicked, this, [command] { command(uiText(u"toggleMesh"), uiText(u"\u7f51\u683c\u663e\u793a\u5c42\u5207\u6362\u8bf7\u6c42\u5df2\u63d0\u4ea4\u3002")); });
    connect(contourButton, &QToolButton::clicked, this, [command] { command(uiText(u"toggleContour"), uiText(u"\u4e91\u56fe\u663e\u793a\u5c42\u5207\u6362\u8bf7\u6c42\u5df2\u63d0\u4ea4\u3002")); });
    connect(sliceButton, &QToolButton::clicked, sliceAction, &QAction::trigger);
    connect(isoButton, &QToolButton::clicked, isoAction, &QAction::trigger);
    connect(streamButton, &QToolButton::clicked, streamAction, &QAction::trigger);
    connect(fitButton, &QToolButton::clicked, fitAction, &QAction::trigger);
    connect(exportButton, &QToolButton::clicked, exportAction, &QAction::trigger);
    connect(deepSeekButton, &QToolButton::clicked, this, openDeepSeekHarness);
}

QWidget *PostProcessWorkbenchWindow::createDataPanel()
{
    auto *panel = new QWidget(this);
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(7, 7, 7, 7);
    auto *titleRow = new QHBoxLayout;
    titleRow->addWidget(sectionLabel(uiText(u"\u5f53\u524d\u540e\u5904\u7406\u4f1a\u8bdd"), panel));
    titleRow->addStretch();
    auto *addResult = new QPushButton(uiText(u"\uff0b \u5bfc\u5165"), panel);
    addResult->setObjectName(uiText(u"primaryAction"));
    titleRow->addWidget(addResult);
    layout->addLayout(titleRow);
    auto *datasetCard = new QFrame(panel);
    datasetCard->setObjectName(uiText(u"dataCard"));
    auto *cardLayout = new QVBoxLayout(datasetCard);
    sessionContext_ = new QLabel(datasetCard);
    sessionContext_->setObjectName(uiText(u"postProcessSessionContext"));
    sessionContext_->setStyleSheet(uiText(u"font-weight: 600; color: #25364c;"));
    sessionContext_->setWordWrap(true);
    auto *datasetStatus = new QLabel(uiText(u"\u5f00\u53d1\u671f Mock \u5165\u53e3\uff1b\u771f\u5b9e\u5de5\u7a0b\u9009\u62e9\u5c06\u66ff\u6362\u6b64\u5904"), datasetCard);
    datasetStatus->setObjectName(uiText(u"mutedText"));
    datasetStatus->setWordWrap(true);
    cardLayout->addWidget(sessionContext_);
    cardLayout->addWidget(datasetStatus);
    layout->addWidget(datasetCard);
    layout->addWidget(sectionLabel(uiText(u"Mock \u9009\u62e9\u5165\u53e3"), panel));
    mockEntrySelector_ = new QComboBox(panel);
    mockEntrySelector_->setObjectName(uiText(u"postProcessMockEntry"));
    mockEntrySelector_->addItems({
        uiText(u"\u65e0\u6709\u6548\u7ed3\u679c"), uiText(u"\u7ed3\u679c\u96c6 / \u4f53\u7f51\u683c"),
        uiText(u"Zone / \u7ec4\u4ef6"), uiText(u"\u8fb9\u754c / \u8868\u9762"),
        uiText(u"\u5b57\u6bb5 Pressure"), uiText(u"\u70b9 / \u5355\u5143"),
        uiText(u"\u65f6\u5e8f\u7ed3\u679c\u96c6"), uiText(u"\u65f6\u95f4\u5e27 t = 1.0 s"),
        uiText(u"\u6536\u655b / \u76d1\u63a7\u8bb0\u5f55")});
    layout->addWidget(mockEntrySelector_);
    sessionOutline_ = new QListWidget(panel);
    sessionOutline_->setObjectName(uiText(u"postProcessSessionOutline"));
    sessionOutline_->setSelectionMode(QAbstractItemView::NoSelection);
    layout->addWidget(sessionOutline_, 1);
    auto *hint = new QLabel(uiText(u"\u4ec5\u5c55\u793a\u5f53\u524d\u4f1a\u8bdd\u7684\u8f93\u5165\u5bf9\u8c61\u4e0e\u6d3e\u751f\u5bf9\u8c61\uff0c\u4e0d\u662f\u5168\u5c40\u5de5\u7a0b\u6811\u3002"), panel);
    hint->setWordWrap(true);
    hint->setStyleSheet(uiText(u"color: #66788a; padding: 4px;"));
    layout->addWidget(hint);
    connect(addResult, &QPushButton::clicked, this, [this] {
        emit postProcessCommandRequested(uiText(u"importResults"));
        appendMessage(uiText(u"\u5bfc\u5165\u7ed3\u679c\u8bf7\u6c42\u5df2\u63d0\u4ea4\uff1b\u8bfb\u53d6\u670d\u52a1\u63a5\u5165\u540e\u5c06\u5728\u5de5\u4f5c\u7ebf\u7a0b\u52a0\u8f7d\u3002"));
    });
    connect(mockEntrySelector_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PostProcessWorkbenchWindow::activateMockEntry);
    return panel;
}

QWidget *PostProcessWorkbenchWindow::createScenePanel()
{
    auto *panel = new QWidget(this);
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(7, 7, 7, 7);
    auto *titleRow = new QHBoxLayout;
    titleRow->addWidget(sectionLabel(uiText(u"\u663e\u793a\u56fe\u5c42"), panel));
    titleRow->addStretch();
    auto *newLayer = new QPushButton(uiText(u"\uff0b \u65b0\u5efa"), panel);
    titleRow->addWidget(newLayer);
    layout->addLayout(titleRow);
    auto *subtitle = new QLabel(uiText(u"\u56fe\u5c42\u662f\u5e73\u94fa\u5bf9\u8c61\uff0c\u53ef\u5355\u72ec\u663e\u9690\u548c\u7f16\u8f91\u3002"), panel);
    subtitle->setObjectName(uiText(u"mutedText"));
    layout->addWidget(subtitle);
    sceneList_ = new QListWidget(panel);
    sceneList_->setObjectName(uiText(u"sceneLayerList"));
    sceneList_->setSelectionMode(QAbstractItemView::SingleSelection);
    const QStringList layers = {uiText(u"\u25c9  \u539f\u59cb\u7f51\u683c     \u53ef\u89c1"), uiText(u"\u25cc  \u4e91\u56fe         \u672a\u521b\u5efa"),
                                uiText(u"\u2571  \u5207\u9762         \u672a\u521b\u5efa"), uiText(u"\u25c7  \u7b49\u503c\u9762       \u672a\u521b\u5efa"),
                                uiText(u"\u2248  \u6d41\u7ebf         \u672a\u521b\u5efa")};
    sceneList_->addItems(layers);
    sceneList_->setCurrentRow(0);
    layout->addWidget(sceneList_, 1);
    auto *actions = new QHBoxLayout;
    auto *showButton = new QPushButton(uiText(u"\u663e\u9690"), panel);
    auto *removeButton = new QPushButton(uiText(u"\u5220\u9664"), panel);
    actions->addWidget(showButton);
    actions->addWidget(removeButton);
    layout->addLayout(actions);
    connect(newLayer, &QPushButton::clicked, this, [this] {
        emit postProcessCommandRequested(uiText(u"newLayer"));
        appendMessage(uiText(u"\u65b0\u5efa\u663e\u793a\u56fe\u5c42\u8bf7\u6c42\u5df2\u63d0\u4ea4\u3002"));
    });
    connect(sceneList_, &QListWidget::itemSelectionChanged, this, &PostProcessWorkbenchWindow::updateSelectionDetails);
    connect(showButton, &QPushButton::clicked, this, [this] { appendMessage(uiText(u"\u573a\u666f\u5bf9\u8c61\u663e\u9690\u8bf7\u6c42\u5df2\u8bb0\u5f55\u3002")); });
    connect(removeButton, &QPushButton::clicked, this, [this] { appendMessage(uiText(u"\u573a\u666f\u5bf9\u8c61\u5220\u9664\u8bf7\u6c42\u5df2\u8bb0\u5f55\u3002")); });
    return panel;
}

QWidget *PostProcessWorkbenchWindow::createViewportPanel()
{
    auto *frame = new QFrame(this);
    frame->setObjectName(uiText(u"viewportFrame"));
    viewportLayout_ = new QVBoxLayout(frame);
    viewportLayout_->setContentsMargins(0, 0, 0, 0);

    emptyViewport_ = new QWidget(frame);
    emptyViewport_->setObjectName(uiText(u"vtkViewportPlaceholder"));
    auto *layout = new QVBoxLayout(emptyViewport_);
    layout->setContentsMargins(20, 20, 20, 20);
    auto *viewTools = new QHBoxLayout;
    viewportTitle_ = new QLabel(uiText(u"\u4e09\u7ef4\u7ed3\u679c\u89c6\u53e3"), emptyViewport_);
    viewportTitle_->setStyleSheet(uiText(u"font-weight: bold; color: #e8f0f8;"));
    viewTools->addWidget(viewportTitle_);
    viewTools->addStretch();
    for (const QString &text : {uiText(u"\u6b63\u89c6"), uiText(u"\u4fef\u89c6"), uiText(u"\u4fa7\u89c6"), uiText(u"\u91cd\u7f6e")}) {
        auto *button = new QPushButton(text, emptyViewport_);
        button->setObjectName(uiText(u"viewportControl"));
        viewTools->addWidget(button);
        connect(button, &QPushButton::clicked, this, [this, text] {
            const QString orientation = text == uiText(u"\u6b63\u89c6") ? uiText(u"front")
                : text == uiText(u"\u4fef\u89c6") ? uiText(u"top")
                : text == uiText(u"\u4fa7\u89c6") ? uiText(u"side") : uiText(u"reset");
            emit viewRequested(orientation);
            appendMessage(uiText(u"%1\u89c6\u56fe\u547d\u4ee4\u5df2\u63d0\u4ea4\u3002").arg(text));
        });
    }
    layout->addLayout(viewTools);
    layout->addStretch();
    viewportHint_ = new QLabel(emptyViewport_);
    viewportHint_->setObjectName(uiText(u"viewportEmptyText"));
    viewportHint_->setAlignment(Qt::AlignCenter);
    viewportHint_->setWordWrap(true);
    viewportHint_->setStyleSheet(uiText(u"font-size: 18px; color: #c9d6e4;"));
    layout->addWidget(viewportHint_, 1);
    auto *overlay = new QLabel(uiText(u"X  \u2192     Y  \u2191     Z  \u2299"), emptyViewport_);
    overlay->setStyleSheet(uiText(u"color: #9fb5c9;"));
    layout->addWidget(overlay, 0, Qt::AlignRight);
    emptyViewport_->setStyleSheet(uiText(u"background: #182432;"));
    viewportLayout_->addWidget(emptyViewport_);
    return frame;
}

QWidget *PostProcessWorkbenchWindow::createPropertiesPanel()
{
    auto *panel = new QWidget(this);
    panel->setMinimumWidth(280);
    panel->setMaximumWidth(360);
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(7, 7, 7, 7);
    layout->addWidget(sectionLabel(uiText(u"\u5c5e\u6027\u4e0e\u663e\u793a"), panel));
    propertiesStack_ = new QStackedWidget(panel);
    propertiesStack_->setObjectName(uiText(u"postProcessProperties"));

    auto *empty = new QLabel(uiText(u"\u5728\u5de6\u4fa7\u56fe\u5c42\u4e2d\u9009\u62e9\u5bf9\u8c61\n\u4ee5\u7f16\u8f91\u5176\u663e\u793a\u4e0e\u53c2\u6570\u3002"), propertiesStack_);
    empty->setAlignment(Qt::AlignCenter);
    empty->setWordWrap(true);
    propertiesStack_->addWidget(empty);

    auto *details = new QWidget(propertiesStack_);
    auto *detailsLayout = new QVBoxLayout(details);
    propertyContextTitle_ = new QLabel(details);
    propertyContextTitle_->setObjectName(uiText(u"postProcessContextTitle"));
    propertyContextTitle_->setStyleSheet(uiText(u"font-size: 15px; font-weight: 600; color: #234d7a;"));
    propertyContextTitle_->setWordWrap(true);
    propertyContextHint_ = new QLabel(details);
    propertyContextHint_->setObjectName(uiText(u"postProcessContextHint"));
    propertyContextHint_->setObjectName(uiText(u"mutedText"));
    propertyContextHint_->setWordWrap(true);
    detailsLayout->addWidget(propertyContextTitle_);
    detailsLayout->addWidget(propertyContextHint_);
    auto *identity = new QGroupBox(uiText(u"\u5bf9\u8c61"), details);
    auto *identityForm = new QFormLayout(identity);
    selectionName_ = new QLineEdit(identity);
    selectionName_->setReadOnly(true);
    identityForm->addRow(uiText(u"\u540d\u79f0"), selectionName_);
    auto *visible = new QCheckBox(uiText(u"\u5728\u4e09\u7ef4\u89c6\u53e3\u4e2d\u663e\u793a"), identity);
    visible->setChecked(true);
    identityForm->addRow(visible);
    detailsLayout->addWidget(identity);

    auto *display = new QGroupBox(uiText(u"\u663e\u793a"), details);
    auto *displayForm = new QFormLayout(display);
    auto *field = new QComboBox(display);
    field->addItems({uiText(u"\u672a\u9009\u62e9\u5b57\u6bb5"), uiText(u"\u538b\u529b"), uiText(u"\u901f\u5ea6\u6a21"), uiText(u"\u6e29\u5ea6")});
    auto *style = new QComboBox(display);
    style->addItems({uiText(u"\u4e91\u56fe"), uiText(u"\u7f51\u683c\u7ebf"), uiText(u"\u8868\u9762"), uiText(u"\u8fb9\u7ebf"), uiText(u"\u77e2\u91cf")});
    auto *opacity = new QSlider(Qt::Horizontal, display);
    opacity->setRange(0, 100);
    opacity->setValue(100);
    displayForm->addRow(uiText(u"\u5b57\u6bb5"), field);
    displayForm->addRow(uiText(u"\u663e\u793a\u65b9\u5f0f"), style);
    displayForm->addRow(uiText(u"\u4e0d\u900f\u660e\u5ea6"), opacity);
    detailsLayout->addWidget(display);

    auto *parameters = new QGroupBox(uiText(u"\u7279\u5f81\u53c2\u6570"), details);
    auto *parameterForm = new QFormLayout(parameters);
    auto *value = new QLineEdit(uiText(u"\u5f85\u9009\u62e9\u5bf9\u8c61"), parameters);
    value->setReadOnly(true);
    parameterForm->addRow(uiText(u"\u5207\u9762 / \u7b49\u503c\u503c"), value);
    detailsLayout->addWidget(parameters);
    detailsLayout->addStretch();
    propertiesStack_->addWidget(details);
    layout->addWidget(propertiesStack_, 1);
    return panel;
}

QWidget *PostProcessWorkbenchWindow::createTimeline()
{
    auto *frame = new QFrame(this);
    frame->setObjectName(uiText(u"postProcessTimeline"));
    auto *layout = new QHBoxLayout(frame);
    layout->setContentsMargins(8, 5, 8, 5);
    layout->addWidget(new QLabel(uiText(u"\u65f6\u95f4\u5e8f\u5217"), frame));
    for (const QString &text : {uiText(u"|\u25c0"), uiText(u"\u25c0"), uiText(u"\u25b6"), uiText(u"\u25b6|")}) {
        auto *button = new QPushButton(text, frame);
        button->setFixedWidth(36);
        layout->addWidget(button);
        connect(button, &QPushButton::clicked, this, [this] { appendMessage(uiText(u"\u65f6\u95f4\u5e27\u5207\u6362\u8bf7\u6c42\u5df2\u8bb0\u5f55\u3002")); });
    }
    timeSlider_ = new QSlider(Qt::Horizontal, frame);
    timeSlider_->setObjectName(uiText(u"postProcessTimeSlider"));
    timeSlider_->setRange(0, 0);
    timeSlider_->setEnabled(false);
    layout->addWidget(timeSlider_, 1);
    timeLabel_ = new QLabel(uiText(u"\u672a\u52a0\u8f7d\u65f6\u5e8f\u7ed3\u679c"), frame);
    layout->addWidget(timeLabel_);
    return frame;
}

void PostProcessWorkbenchWindow::setFeatureButtonsEnabled(bool mesh, bool contour, bool slice,
                                                          bool iso, bool streamline)
{
    const bool states[] = {mesh, contour, slice, iso, streamline};
    for (int i = 0; i < featureButtons_.size() && i < 5; ++i)
        featureButtons_.at(i)->setEnabled(states[i]);
}

void PostProcessWorkbenchWindow::activateMockEntry(int index)
{
    if (!mockEntrySelector_ || !sessionOutline_ || !sceneList_ || !propertiesStack_)
        return;

    QString context;
    QString title;
    QString description;
    QString viewport;
    QStringList outline;
    QStringList layers;
    bool timeMode = false;
    bool curveMode = false;
    bool mesh = true;
    bool contour = true;
    bool slice = true;
    bool iso = true;
    bool streamline = true;

    switch (index) {
    case 0:
        context = uiText(u"\u65e0\u6709\u6548\u7ed3\u679c \u00b7 Empty");
        title = uiText(u"\u7b49\u5f85\u540e\u5904\u7406\u8f93\u5165");
        description = uiText(u"\u6ca1\u6709\u7ed1\u5b9a\u7f51\u683c\u3001\u7ed3\u679c\u6216\u76d1\u63a7\u8bb0\u5f55\u3002");
        viewport = uiText(u"\u8bf7\u4ece\u5de5\u7a0b\u4e2d\u9009\u4e2d\u7ed3\u679c\u5bf9\u8c61\u540e\u8fdb\u5165\u540e\u5904\u7406\u3002");
        outline << uiText(u"\u25cb  \u6682\u65e0\u540e\u5904\u7406\u8f93\u5165");
        mesh = contour = slice = iso = streamline = false;
        break;
    case 1:
        context = uiText(u"MockCase-A / \u7ed3\u679c\u96c6 / \u4f53\u7f51\u683c");
        title = uiText(u"Spatial \u6a21\u5f0f\uff1a\u7ed3\u679c\u96c6");
        description = uiText(u"\u53ef\u5728\u4f53\u7f51\u683c\u4e0a\u6267\u884c\u7f51\u683c\u3001\u4e91\u56fe\u3001\u5207\u9762\u3001\u7b49\u503c\u9762\u548c\u4f53\u6d41\u7ebf\u64cd\u4f5c\u3002");
        viewport = uiText(u"Spatial \u6a21\u5f0f\n\n\u5df2\u9009\u4e2d\u4f53\u7f51\u683c\u7ed3\u679c\u96c6\u3002\nVTK \u89c6\u53e3\u63a5\u5165\u540e\u5c06\u5728\u6b64\u663e\u793a\u3002");
        outline << uiText(u"\u25a6  Volume Mesh") << uiText(u"\u25c9  Pressure, Temperature, Velocity")
                << uiText(u"\u25a7  Zone: Flow Volume");
        layers << uiText(u"\u25c9  \u4f53\u7f51\u683c     \u53ef\u89c1") << uiText(u"\u25cc  \u4e91\u56fe         \u5f85\u521b\u5efa");
        break;
    case 2:
        context = uiText(u"MockCase-A / Wing Zone");
        title = uiText(u"Spatial \u6a21\u5f0f\uff1aZone / \u7ec4\u4ef6");
        description = uiText(u"\u4e09\u7ef4\u89c6\u53e3\u805a\u7126\u5f53\u524d Zone\uff0c\u5176\u4ed6 Zone \u7531\u573a\u666f\u670d\u52a1\u51b3\u5b9a\u663e\u9690\u3002");
        viewport = uiText(u"Zone Focus \u6a21\u5f0f\n\n\u5df2\u9009\u4e2d Wing Zone\u3002");
        outline << uiText(u"\u25a6  Zone: Wing") << uiText(u"\u25c9  Pressure, Velocity") << uiText(u"\u25a7  Wall Boundary");
        layers << uiText(u"\u25c9  Wing Zone      \u53ef\u89c1") << uiText(u"\u25cb  \u5176\u4ed6 Zone      \u6de1\u5316");
        break;
    case 3:
        context = uiText(u"MockCase-A / Wall Boundary");
        title = uiText(u"Surface \u6a21\u5f0f\uff1a\u8fb9\u754c\u9762");
        description = uiText(u"\u64cd\u4f5c\u53ea\u4f5c\u7528\u4e8e\u5f53\u524d\u8fb9\u754c\u9762\uff0c\u4e0d\u5bf9\u6574\u4e2a\u4f53\u7f51\u683c\u521b\u5efa\u7b49\u503c\u9762\u3002");
        viewport = uiText(u"Surface Focus \u6a21\u5f0f\n\n\u5df2\u9009\u4e2d Wall Boundary\u3002");
        outline << uiText(u"\u25a6  Zone: Wing") << uiText(u"\u25a7  Boundary: Wall") << uiText(u"\u25c9  Surface Pressure");
        layers << uiText(u"\u25c9  Wall Boundary  \u53ef\u89c1");
        slice = iso = false;
        break;
    case 4:
        context = uiText(u"MockCase-A / Field: Pressure");
        title = uiText(u"Field Focus\uff1aPressure [Pa]");
        description = uiText(u"\u5f53\u524d\u5b57\u6bb5\u6210\u4e3a\u4e91\u56fe\u3001\u7b49\u503c\u7ebf\u548c\u7b49\u503c\u9762\u7684\u9ed8\u8ba4\u8f93\u5165\u3002");
        viewport = uiText(u"Field Focus \u6a21\u5f0f\n\n\u5f53\u524d\u5b57\u6bb5\uff1aPressure [Pa]\u3002");
        outline << uiText(u"\u25a6  Volume Mesh") << uiText(u"\u25c9  Field: Pressure [Pa]") << uiText(u"\u25f7  \u8272\u6807\u4e0e\u503c\u57df");
        layers << uiText(u"\u25c9  Pressure \u4e91\u56fe   \u5f85\u521b\u5efa");
        break;
    case 5:
        context = uiText(u"MockCase-A / Cell #128");
        title = uiText(u"Probe \u6a21\u5f0f\uff1a\u70b9 / \u5355\u5143\u67e5\u8be2");
        description = uiText(u"\u5f53\u524d\u5355\u5143\u5c06\u5728\u89c6\u53e3\u9ad8\u4eae\uff0c\u53ef\u67e5\u770b ID\u3001\u5750\u6807\u548c\u7269\u7406\u91cf\u3002");
        viewport = uiText(u"Probe \u6a21\u5f0f\n\n\u5df2\u9009\u4e2d Cell #128\u3002");
        outline << uiText(u"\u25a6  Volume Mesh") << uiText(u"\u25ce  Cell #128") << uiText(u"\u25c9  Pressure, Temperature, Velocity");
        layers << uiText(u"\u25ce  Cell #128      \u5f53\u524d\u9009\u4e2d");
        mesh = contour = slice = iso = false;
        break;
    case 6:
    case 7:
        timeMode = true;
        context = index == 6 ? uiText(u"MockCase-A / \u65f6\u5e8f\u7ed3\u679c") : uiText(u"MockCase-A / Frame 1 / t = 1.0 s");
        title = index == 6 ? uiText(u"Spatial + Time \u6a21\u5f0f") : uiText(u"\u65f6\u95f4\u5e27 t = 1.0 s");
        description = uiText(u"\u5e95\u90e8\u65f6\u95f4\u8f74\u63a7\u5236\u5f53\u524d\u5e27\uff0c\u7a7a\u95f4\u529f\u80fd\u4ecd\u4f5c\u7528\u4e8e\u5f53\u524d\u5e27\u3002");
        viewport = index == 6 ? uiText(u"Spatial + Time \u6a21\u5f0f\n\n\u5f53\u524d\u663e\u793a Frame 0\u3002") : uiText(u"Spatial + Time \u6a21\u5f0f\n\n\u5df2\u5b9a\u4f4d Frame 1 / t = 1.0 s\u3002");
        outline << uiText(u"\u25f7  Frame 0  t = 0.0 s") << uiText(u"\u25c9  Frame 1  t = 1.0 s") << uiText(u"\u25f7  Frame 2  t = 2.0 s");
        layers << uiText(u"\u25c9  \u5f53\u524d\u5e27        \u53ef\u89c1");
        break;
    default:
        curveMode = true;
        context = uiText(u"MockCase-A / \u6536\u655b\u4e0e\u76d1\u63a7");
        title = uiText(u"Curve \u6a21\u5f0f\uff1a\u6536\u655b / \u76d1\u63a7\u8bb0\u5f55");
        description = uiText(u"\u4e2d\u592e\u5de5\u4f5c\u533a\u663e\u793a\u4e8c\u7ef4\u66f2\u7ebf\u753b\u5e03\uff0c\u4ecd\u590d\u7528\u540c\u4e00\u4e2a\u540e\u5904\u7406\u4e3b\u6846\u67b6\u3002");
        viewport = uiText(u"Curve \u6a21\u5f0f\n\n\u9009\u4e2d\u6b8b\u503c\u6216\u76d1\u63a7\u8bb0\u5f55\u540e\uff0c\u771f\u5b9e\u66f2\u7ebf\u7ec4\u4ef6\u5c06\u5728\u6b64\u6e32\u67d3\u3002");
        outline << uiText(u"\u223f  Residual: Density") << uiText(u"\u223f  Residual: Energy") << uiText(u"\u223f  Force Coefficient: Cz");
        layers << uiText(u"\u223f  Density Residual") << uiText(u"\u223f  Cz Monitor");
        mesh = contour = slice = iso = streamline = false;
        break;
    }

    sessionContext_->setText(context);
    sessionOutline_->clear();
    sessionOutline_->addItems(outline);
    sceneList_->clear();
    sceneList_->addItems(layers);
    if (!layers.isEmpty()) sceneList_->setCurrentRow(0);
    viewportTitle_->setText(curveMode ? uiText(u"\u6536\u655b\u4e0e\u76d1\u63a7\u66f2\u7ebf") : uiText(u"\u4e09\u7ef4\u7ed3\u679c\u89c6\u53e3"));
    viewportHint_->setText(viewport);
    propertyContextTitle_->setText(title);
    propertyContextHint_->setText(description);
    propertiesStack_->setCurrentIndex(index == 0 ? 0 : 1);
    auto *timeline = findChild<QWidget *>(uiText(u"postProcessTimeline"));
    if (timeline) timeline->setVisible(timeMode);
    if (timeMode) {
        timeSlider_->setRange(0, 2);
        timeSlider_->setEnabled(true);
        timeSlider_->setValue(index == 7 ? 1 : 0);
        timeLabel_->setText(index == 7 ? uiText(u"t = 1.0 s") : uiText(u"Frame 0 / t = 0.0 s"));
    }
    setFeatureButtonsEnabled(mesh, contour, slice, iso, streamline);
}

bool PostProcessWorkbenchWindow::insertWidget(WidgetSlot slot, QWidget *widget)
{
    if (slot != WidgetSlot::Viewport || !widget || !viewportLayout_)
        return false;
    if (emptyViewport_) {
        viewportLayout_->removeWidget(emptyViewport_);
        emptyViewport_->deleteLater();
        emptyViewport_ = nullptr;
    }
    if (viewportLayout_->count() != 0)
        return false;
    viewportLayout_->addWidget(widget);
    return true;
}

void PostProcessWorkbenchWindow::bindServices(const ServiceBindings &services)
{
    if (bound_)
        return;
    services_ = services;
    bound_ = true;
    if (services_.tasks) {
        connect(services_.tasks, &ITaskService::taskEventOccurred, this,
                [this](const TaskEvent &event) { appendMessage(event.message); },
                Qt::QueuedConnection);
    }
}

void PostProcessWorkbenchWindow::appendMessage(const QString &message)
{
    events_->append(message.toHtmlEscaped());
    statusBar()->showMessage(message, 5000);
}

void PostProcessWorkbenchWindow::updateSelectionDetails()
{
    const auto selected = sceneList_->selectedItems();
    if (selected.isEmpty()) {
        propertiesStack_->setCurrentIndex(0);
        return;
    }
    selectionName_->setText(selected.first()->text());
    propertiesStack_->setCurrentIndex(1);
}

} // namespace aerosim
