#include "aerosim/ui/CalculationTaskPage.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QDateTime>
#include <QFontDatabase>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QMenu>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QSplitter>
#include <QTableWidget>
#include <QVBoxLayout>

namespace aerosim {

template <std::size_t N>
QString uiText(const char16_t (&value)[N])
{
    return QString::fromUtf16(reinterpret_cast<const ushort *>(value), int(N) - 1);
}
namespace {
// Empty axes only: no generated samples or curve algorithm. 邱's monitor
// widget replaces this canvas through the existing WidgetSlot::Monitor host.
class EmptyMonitorChart final : public QWidget {
public:
    explicit EmptyMonitorChart(QWidget *parent) : QWidget(parent)
    {
        setObjectName(QStringLiteral("calculationEmptyChart"));
        setMinimumSize(190, 145);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        setAccessibleName(uiText(u"\u76d1\u63a7\u56fe\u50cf\uff0c\u6682\u65e0\u6570\u636e"));
    }
protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.fillRect(rect(), palette().brush(QPalette::Base));
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(palette().color(QPalette::Text));
        const QRectF chart(42, 20, qMax(80, width() - 60), qMax(50, height() - 65));
        painter.drawLine(chart.bottomLeft(), chart.bottomRight());
        painter.drawLine(chart.bottomLeft(), chart.topLeft());
        for (int i = 0; i <= 4; ++i) {
            const qreal x = chart.left() + chart.width() * i / 4;
            const qreal y = chart.bottom() - chart.height() * i / 4;
            painter.drawLine(QPointF(x, chart.bottom()), QPointF(x, chart.bottom() - 4));
            painter.drawLine(QPointF(chart.left(), y), QPointF(chart.left() + 4, y));
            painter.drawText(QRectF(x - 15, chart.bottom() + 5, 30, 18), Qt::AlignCenter, QString::number(i));
            painter.drawText(QRectF(3, y - 10, 30, 20), Qt::AlignRight | Qt::AlignVCenter, QString::number(i));
        }
        painter.drawText(QRectF(chart.left(), height() - 23, chart.width(), 20), Qt::AlignCenter, QStringLiteral("Iteration Steps"));
        painter.setPen(palette().color(QPalette::Disabled, QPalette::Text));
        painter.drawText(chart, Qt::AlignCenter, uiText(u"\u6682\u65e0\u76d1\u63a7\u6570\u636e"));
    }
};

QLabel *sectionTitle(const QString &text, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setProperty("calculationSection", true);
    label->setMinimumHeight(28);
    label->setMargin(6);
    return label;
}

double splitRatio(QSplitter *splitter)
{
    const auto sizes = splitter->sizes();
    const int total = sizes.value(0) + sizes.value(1);
    return total > 0 ? double(sizes[0]) / total : 0.5;
}

void restoreRatio(QSplitter *splitter, double ratio)
{
    ratio = qBound(0.15, ratio, 0.85);
    splitter->setSizes({qRound(1000 * ratio), qRound(1000 * (1 - ratio))});
}
}

CalculationTaskPage::CalculationTaskPage(bool multiple, QWidget *parent)
    : QWidget(parent), multiple_(multiple)
{
    setObjectName(multiple ? QStringLiteral("multipleTaskPage") : QStringLiteral("singleTaskPage"));
    setAttribute(Qt::WA_StyledBackground);
    setStyleSheet(QStringLiteral(
        "QLabel[calculationSection=\"true\"] { background: palette(highlight); color: palette(highlighted-text); }"
        "QGroupBox { border: 1px solid palette(mid); border-radius: 3px; margin-top: 12px; padding: 10px 7px 7px; }"
        "QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top center; padding: 0 5px; }"
        "QSplitter::handle { background: palette(midlight); }"
        "QSplitter::handle:horizontal { width: 5px; }"
        "QSplitter::handle:vertical { height: 5px; }"
        "QSplitter::handle:hover { background: palette(highlight); }"
        "QPushButton[taskCategory=\"true\"] { padding: 6px 3px; }"
        "QLabel#calculationUiNotice { color: palette(mid); padding: 2px 6px; }"));
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    mainSplitter_ = new QSplitter(Qt::Horizontal, this);
    mainSplitter_->setObjectName(QStringLiteral("calculationMainSplitter"));
    mainSplitter_->setChildrenCollapsible(false);
    auto *settings = new QWidget(mainSplitter_);
    settings->setObjectName(QStringLiteral("calculationSettingsPanel"));
    settings->setMinimumWidth(360);
    auto *settingsLayout = new QVBoxLayout(settings);
    settingsLayout->setContentsMargins(0, 0, 0, 0);
    settingsLayout->setSpacing(0);
    settingsLayout->addWidget(sectionTitle(uiText(u"\u8bbe\u7f6e\u9762\u677f"), settings));
    auto *settingsScroll = new QScrollArea(settings);
    settingsScroll->setWidgetResizable(true);
    settingsScroll->setFrameShape(QFrame::NoFrame);
    settingsScroll->setWidget(multiple ? createMultipleSettings() : createSingleSettings());
    settingsLayout->addWidget(settingsScroll);

    rightSplitter_ = new QSplitter(Qt::Vertical, mainSplitter_);
    rightSplitter_->setObjectName(QStringLiteral("calculationRightSplitter"));
    rightSplitter_->setChildrenCollapsible(false);
    rightSplitter_->setMinimumWidth(270);
    auto *monitor = new QWidget(rightSplitter_);
    auto *monitorLayout = new QVBoxLayout(monitor);
    monitorLayout->setContentsMargins(0, 0, 0, 0);
    monitorLayout->setSpacing(0);
    auto *tabs = new QWidget(monitor);
    tabs->setObjectName(QStringLiteral("calculationViewTabs"));
    auto *tabLayout = new QHBoxLayout(tabs);
    tabLayout->setContentsMargins(0, 0, 0, 0);
    auto *tab = new QPushButton(multiple ? uiText(u"\u591a\u5de5\u51b5") : uiText(u"\u5355\u5de5\u51b5"), tabs);
    tab->setObjectName(QStringLiteral("calculationConditionTab"));
    tab->setCheckable(true); tab->setChecked(true);
    connect(tab, &QPushButton::clicked, this, [tab] { tab->setChecked(true); });
    tabLayout->addWidget(tab);
    if (multiple) {
        auto *monitorTab = new QPushButton(uiText(u"\u8ba1\u7b97\u76d1\u63a7"), monitor);
        monitorTab->setObjectName(QStringLiteral("calculationMonitorTab"));
        monitorTab->setCheckable(true); monitorTab->setChecked(true);
        connect(monitorTab, &QPushButton::clicked, this, [monitorTab] { monitorTab->setChecked(true); });
        tabLayout->addWidget(monitorTab);
    }
    tabLayout->addStretch();
    monitorLayout->addWidget(tabs);
    monitorSplitter_ = new QSplitter(Qt::Horizontal, monitor);
    monitorSplitter_->setObjectName(QStringLiteral("calculationMonitorSplitter"));
    monitorSplitter_->setChildrenCollapsible(false);
    if (multiple) {
        auto *observations = new QWidget(monitorSplitter_);
        auto *observationLayout = new QVBoxLayout(observations);
        observationLayout->setContentsMargins(8, 6, 3, 5);
        observationLayout->addWidget(createObservables(), 1);
        auto *samples = new QCheckBox(uiText(u"\u4ec5\u663e\u793a\u6837\u672c\u70b9"), observations);
        samples->setObjectName(QStringLiteral("calculationSamplesOnly"));
        samples->setToolTip(uiText(u"\u9009\u62e9\u4fdd\u7559\u5728\u672c\u9875\uff0c\u63a5\u5165\u66f2\u7ebf\u6570\u636e\u540e\u751f\u6548"));
        observationLayout->addWidget(samples);
    }
    auto *chartPanel = new QWidget(monitorSplitter_);
    chartPanel->setObjectName(QStringLiteral("calculationChartPanel"));
    curveLayout_ = new QVBoxLayout(chartPanel);
    curveLayout_->setContentsMargins(0, 0, 0, 0);
    emptyChart_ = new EmptyMonitorChart(chartPanel);
    curveLayout_->addWidget(emptyChart_);
    monitorLayout->addWidget(monitorSplitter_, 1);

    auto *logPanel = new QWidget(rightSplitter_);
    logPanel->setObjectName(QStringLiteral("calculationLogPanel"));
    logPanel->setMinimumHeight(100);
    auto *logLayout = new QVBoxLayout(logPanel);
    logLayout->setContentsMargins(0, 0, 0, 0); logLayout->setSpacing(0);
    logLayout->addWidget(sectionTitle(uiText(u"\u65e5\u5fd7"), logPanel));
    log_ = new QPlainTextEdit(logPanel);
    log_->setObjectName(QStringLiteral("calculationLog"));
    log_->setReadOnly(true);
    log_->setMaximumBlockCount(2000);
    log_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    log_->setPlaceholderText(uiText(u"\u6682\u65e0\u8fd0\u884c\u65e5\u5fd7"));
    logLayout->addWidget(log_);
    mainSplitter_->setStretchFactor(0, 0);
    mainSplitter_->setStretchFactor(1, 1);
    layout->addWidget(mainSplitter_, 1);
    auto *notice = new QLabel(uiText(u"\u754c\u9762\u9884\u89c8 \u00b7 \u5c1a\u672a\u63a5\u5165\u8ba1\u7b97\u4e0e\u6587\u4ef6\u751f\u6210"), this);
    notice->setObjectName(QStringLiteral("calculationUiNotice"));
    layout->addWidget(notice);
    restoreDefaultLayout();
}

QWidget *CalculationTaskPage::createSingleSettings()
{
    auto *panel = new QWidget(this);
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(12);
    auto *calculation = new QGroupBox(uiText(u"\u8ba1\u7b97"), panel);
    auto *calculationLayout = new QHBoxLayout(calculation);
    calculationLayout->addWidget(new QLabel(uiText(u"\u7eed\u7b97\uff1a"), calculation));
    auto *resume = new QCheckBox(calculation);
    resume->setObjectName(QStringLiteral("singleTaskResume"));
    resume->setAccessibleName(uiText(u"\u7eed\u7b97"));
    calculationLayout->addWidget(resume); calculationLayout->addStretch();
    layout->addWidget(calculation);
    auto *execution = new QGroupBox(uiText(u"\u6267\u884c\u65b9\u5f0f"), panel);
    auto *executionLayout = new QHBoxLayout(execution);
    executionLayout->addWidget(new QLabel(uiText(u"\u8fd0\u884c\u6a21\u5f0f\uff1a"), execution));
    auto *window = new QRadioButton(QStringLiteral("Window"), execution);
    window->setObjectName(QStringLiteral("singleTaskWindowMode")); window->setChecked(true);
    auto *console = new QRadioButton(QStringLiteral("Console"), execution);
    console->setObjectName(QStringLiteral("singleTaskConsoleMode"));
    executionLayout->addWidget(window); executionLayout->addWidget(console); executionLayout->addStretch();
    layout->addWidget(execution);
    layout->addWidget(createSubmissionControls());
    layout->addSpacing(10);
    auto *observables = createObservables();
    observables->setMaximumHeight(135);
    layout->addWidget(observables);
    layout->addStretch();
    return panel;
}

QWidget *CalculationTaskPage::createMultipleSettings()
{
    auto *panel = new QWidget(this);
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(10, 8, 10, 10);
    auto *group = new QGroupBox(uiText(u"\u591a\u72b6\u6001"), panel);
    auto *groupLayout = new QVBoxLayout(group);
    table_ = new QTableWidget(0, 8, group);
    table_->setObjectName(QStringLiteral("multipleTaskTable"));
    table_->setHorizontalHeaderLabels({uiText(u"\u5e8f\u53f7"), uiText(u"\u8ba1\u7b97\u72b6\u6001"), uiText(u"\u7eed\u7b97"),
        uiText(u"\u8fed\u4ee3\u6b65\u6570"), uiText(u"\u53c2\u8003\u4fe1\u606f"), uiText(u"\u9a6c\u8d6b\u6570"), uiText(u"\u653b\u89d2"), uiText(u"\u4fa7\u6ed1\u89d2")});
    table_->verticalHeader()->hide();
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setAlternatingRowColors(true);
    table_->setMinimumHeight(185);
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    table_->horizontalHeader()->setMinimumSectionSize(40);
    const int widths[] = {42, 62, 38, 58, 95, 52, 48, 52};
    for (int col = 0; col < 8; ++col) table_->setColumnWidth(col, widths[col]);
    groupLayout->addWidget(table_, 1);
    auto *types = new QWidget(group);
    types->setObjectName(QStringLiteral("taskCategoryButtons"));
    auto *typeLayout = new QGridLayout(types);
    typeLayout->setContentsMargins(0, 4, 0, 4);
    const CalculationInputKind kinds[] = {CalculationInputKind::ReynoldsTemperature,
        CalculationInputKind::PressureTemperature, CalculationInputKind::TotalPressureTemperature,
        CalculationInputKind::Altitude, CalculationInputKind::Trajectory};
    for (int i = 0; i < 5; ++i) {
        const auto kind = kinds[i];
        auto *button = new QPushButton(calculationInputTitle(kind), types);
        button->setObjectName(QStringLiteral("addTaskCategory_%1").arg(i));
        button->setProperty("taskCategory", true);
        button->setToolTip(uiText(u"\u6dfb\u52a0%1\u4efb\u52a1").arg(calculationInputTitle(kind)));
        connect(button, &QPushButton::clicked, this, [this, kind] { addTasks(kind); });
        // Two rows keep the full category names legible on laptop displays.
        typeLayout->addWidget(button, i / 3, i % 3);
    }
    groupLayout->addWidget(types);
    auto *commands = new QHBoxLayout;
    selectAll_ = new QCheckBox(uiText(u"\u5168\u9009"), group);
    selectAll_->setObjectName(QStringLiteral("selectAllTasks"));
    commands->addWidget(selectAll_);
    commands->addStretch();
    auto *add = new QPushButton(uiText(u"+ \u6dfb\u52a0\u4efb\u52a1"), group);
    add->setObjectName(QStringLiteral("addTask"));
    add->setToolTip(uiText(u"\u6253\u5f00\u4efb\u52a1\u7c7b\u522b\u83dc\u5355\uff1b\u4e5f\u53ef\u4ee5\u76f4\u63a5\u70b9\u51fb\u4e0b\u65b9\u7c7b\u522b\u6309\u94ae"));
    commands->addWidget(add);
    remove_ = new QPushButton(uiText(u"\u2212 \u5220\u9664\u9009\u4e2d"), group);
    remove_->setObjectName(QStringLiteral("removeSelectedTasks"));
    remove_->setToolTip(uiText(u"\u5220\u9664\u52fe\u9009\u4efb\u52a1\uff1b\u6ca1\u6709\u52fe\u9009\u65f6\u5220\u9664\u9ad8\u4eae\u884c"));
    clear_ = new QPushButton(uiText(u"\u6e05\u7a7a"), group);
    clear_->setObjectName(QStringLiteral("clearTasks"));
    commands->addWidget(remove_); commands->addWidget(clear_);
    groupLayout->addLayout(commands);
    count_ = new QLabel(group);
    count_->setObjectName(QStringLiteral("multipleTaskCount"));
    groupLayout->addWidget(count_);
    layout->addWidget(group, 1);
    layout->addWidget(createSubmissionControls());
    connect(table_, &QTableWidget::cellDoubleClicked, this, [this](int row, int col) {
        if (col != 0 && col != 2 && col != 3) editTask(row);
    });
    connect(table_, &QTableWidget::itemChanged, this, [this] { updateSelection(); });
    connect(table_, &QTableWidget::itemSelectionChanged, this, &CalculationTaskPage::updateSelection);
    connect(selectAll_, &QCheckBox::clicked, this, [this](bool checked) {
        const QSignalBlocker blocker(table_);
        for (int row = 0; row < table_->rowCount(); ++row)
            table_->item(row, 0)->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
        updateSelection();
    });
    connect(remove_, &QPushButton::clicked, this, &CalculationTaskPage::removeSelected);
    connect(add, &QPushButton::clicked, this, [this, add] {
        QMenu menu(add);
        const CalculationInputKind kinds[] = {CalculationInputKind::ReynoldsTemperature,
            CalculationInputKind::PressureTemperature, CalculationInputKind::TotalPressureTemperature,
            CalculationInputKind::Altitude, CalculationInputKind::Trajectory};
        for (const auto kind : kinds) {
            const auto selectedKind = kind;
            auto *action = menu.addAction(calculationInputTitle(selectedKind));
            connect(action, &QAction::triggered, this, [this, selectedKind] { addTasks(selectedKind); });
        }
        menu.exec(add->mapToGlobal(QPoint(0, add->height())));
    });
    connect(clear_, &QPushButton::clicked, this, [this] {
        if (QMessageBox::question(this, uiText(u"\u6e05\u7a7a\u4efb\u52a1"), uiText(u"\u6e05\u7a7a\u672c\u9875\u7684\u5168\u90e8\u4efb\u52a1\uff1f"),
                                 QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) return;
        rows_.clear(); table_->setRowCount(0); updateSelection();
        appendLog(uiText(u"\u5df2\u6e05\u7a7a\u672c\u9875\u4efb\u52a1\u3002"));
    });
    updateSelection();
    return panel;
}

QWidget *CalculationTaskPage::createSubmissionControls()
{
    auto *widget = new QWidget(this);
    auto *layout = new QVBoxLayout(widget);
    layout->setContentsMargins(0, 5, 0, 10);
    auto *modes = new QButtonGroup(widget);
    const QStringList labels = {uiText(u"\u63d0\u4ea4\u672c\u5730\u8ba1\u7b97"), uiText(u"\u4ec5\u751f\u6210xdb\u6587\u4ef6"), uiText(u"\u4ec5\u751f\u6210\u8ba1\u7b97\u538b\u7f29\u5305")};
    // A grid wraps the longer third choice instead of forcing the settings
    // splitter to consume the plot area at common desktop resolutions.
    auto *choices = new QGridLayout;
    for (int i = 0; i < labels.size(); ++i) {
        auto *radio = new QRadioButton(labels[i], widget);
        radio->setObjectName(QStringLiteral("submissionMode_%1").arg(i));
        modes->addButton(radio, i); choices->addWidget(radio, i / 2, i % 2);
        if (i == 0) radio->setChecked(true);
    }
    layout->addLayout(choices);
    auto *submit = new QPushButton(uiText(u"\u63d0\u4ea4\u8ba1\u7b97"), widget);
    submit->setObjectName(QStringLiteral("submitCalculation"));
    submit->setToolTip(uiText(u"\u754c\u9762\u9884\u89c8\uff1a\u8ba1\u7b97\u6267\u884c\u4e0e\u6587\u4ef6\u751f\u6210\u5c1a\u672a\u63a5\u5165"));
    layout->addWidget(submit, 0, Qt::AlignHCenter);
    connect(submit, &QPushButton::clicked, this, [this, modes] {
        if (multiple_) {
            int checked = 0;
            for (int row = 0; row < table_->rowCount(); ++row)
                checked += table_->item(row, 0)->checkState() == Qt::Checked;
            if (!checked) { appendLog(uiText(u"\u8bf7\u5148\u6dfb\u52a0\u5e76\u52fe\u9009\u9700\u8981\u63d0\u4ea4\u7684\u4efb\u52a1\u3002")); return; }
        }
        appendLog(uiText(u"\u5df2\u9009\u62e9\u201c%1\u201d\u3002\u5f53\u524d\u4e3a\u754c\u9762\u9884\u89c8\uff0c\u5c1a\u672a\u63d0\u4ea4\u8ba1\u7b97\u6216\u751f\u6210\u6587\u4ef6\u3002")
                      .arg(modes->checkedButton()->text()));
    });
    return widget;
}

QWidget *CalculationTaskPage::createObservables()
{
    auto *group = new QGroupBox(uiText(u"\u89c2\u6d4b\u91cf"), this);
    group->setObjectName(QStringLiteral("calculationObservables"));
    group->setMinimumSize(100, 70);
    auto *layout = new QVBoxLayout(group);
    auto *placeholder = new QLabel(uiText(u"\u6682\u65e0\u89c2\u6d4b\u91cf"), group);
    placeholder->setAlignment(Qt::AlignCenter);
    placeholder->setEnabled(false);
    layout->addWidget(placeholder);
    return group;
}

void CalculationTaskPage::addTasks(CalculationInputKind kind)
{
    CalculationTaskInputDialog dialog(kind, this);
    if (dialog.exec() != QDialog::Accepted) return;
    const auto newRows = dialog.rows();
    if (rows_.size() + newRows.size() > 1000) {
        appendLog(uiText(u"\u672c\u9875\u6700\u591a\u4fdd\u7559 1000 \u4e2a\u4efb\u52a1\uff0c\u8bf7\u5220\u9664\u90e8\u5206\u4efb\u52a1\u540e\u518d\u6dfb\u52a0\u3002")); return;
    }
    const int first = rows_.size();
    for (const auto &values : newRows) {
        rows_.append({kind, values, nextId_++});
        table_->insertRow(table_->rowCount());
        fillRow(table_->rowCount() - 1);
    }
    table_->selectRow(first);
    updateSelection();
    appendLog(uiText(u"\u5df2\u6dfb\u52a0 %1 \u4e2a\u201c%2\u201d\u4efb\u52a1\uff0c\u5c1a\u672a\u63d0\u4ea4\u3002 ").arg(newRows.size()).arg(calculationInputTitle(kind)));
}

void CalculationTaskPage::editTask(int row)
{
    if (row < 0 || row >= rows_.size()) return;
    CalculationTaskInputDialog dialog(rows_[row].kind, this);
    dialog.setValues(rows_[row].values);
    if (dialog.exec() != QDialog::Accepted) return;
    rows_[row].values = dialog.rows().first();
    fillRow(row); updateSelection();
    appendLog(uiText(u"\u5df2\u66f4\u65b0\u4efb\u52a1 %1\u3002 ").arg(rows_[row].id));
}

void CalculationTaskPage::fillRow(int row)
{
    const QSignalBlocker blocker(table_);
    const auto &entry = rows_[row];
    const auto labels = calculationInputLabels(entry.kind);
    const int references = entry.values.size() - 3;
    QStringList info;
    for (int i = 0; i < references; ++i)
        info << labels[i] + QStringLiteral(": ") + QString::number(entry.values[i], 'g', 10);
    const QStringList texts = {QString::number(entry.id), uiText(u"\u5f85\u8ba1\u7b97"), QString(), QString(), info.join('\n'),
        QString::number(entry.values[references], 'g', 10), QString::number(entry.values[references + 1], 'g', 10),
        QString::number(entry.values[references + 2], 'g', 10)};
    for (int col = 0; col < texts.size(); ++col) {
        auto *item = table_->item(row, col);
        if (!item) {
            item = new QTableWidgetItem;
            table_->setItem(row, col, item);
            if (col == 0 || col == 2) item->setCheckState(Qt::Unchecked);
        }
        item->setText(texts[col]);
        item->setToolTip(calculationInputTitle(entry.kind) + QStringLiteral("\n") + info.join('\n'));
    }
    if (!table_->cellWidget(row, 3)) {
        auto *iterations = new QSpinBox(table_);
        iterations->setRange(1, 100000000); iterations->setValue(100);
        iterations->setFrame(false);
        table_->setCellWidget(row, 3, iterations);
    }
    table_->setRowHeight(row, references > 1 ? 52 : 36);
}

void CalculationTaskPage::updateSelection()
{
    int checked = 0;
    for (int row = 0; row < table_->rowCount(); ++row)
        if (table_->item(row, 0) && table_->item(row, 0)->checkState() == Qt::Checked) ++checked;
    const QSignalBlocker blocker(selectAll_);
    selectAll_->setTristate(checked > 0 && checked < table_->rowCount());
    selectAll_->setCheckState(checked == 0 ? Qt::Unchecked
        : checked == table_->rowCount() ? Qt::Checked : Qt::PartiallyChecked);
    selectAll_->setEnabled(table_->rowCount() > 0);
    remove_->setEnabled(checked > 0 || !table_->selectionModel()->selectedRows().isEmpty());
    clear_->setEnabled(table_->rowCount() > 0);
    count_->setText(uiText(u"\u5171 %1 \u4e2a\u4efb\u52a1 \u00b7 \u5df2\u52fe\u9009 %2 \u4e2a").arg(table_->rowCount()).arg(checked));
}

void CalculationTaskPage::removeSelected()
{
    QVector<int> selected;
    for (int row = 0; row < table_->rowCount(); ++row)
        if (table_->item(row, 0)->checkState() == Qt::Checked) selected.append(row);
    if (selected.isEmpty())
        for (int row = 0; row < table_->rowCount(); ++row)
            if (table_->selectionModel()->isRowSelected(row, QModelIndex())) selected.append(row);
    for (int i = selected.size() - 1; i >= 0; --i) {
        rows_.removeAt(selected[i]); table_->removeRow(selected[i]);
    }
    updateSelection();
    appendLog(uiText(u"\u5df2\u5220\u9664 %1 \u4e2a\u4efb\u52a1\u3002 ").arg(selected.size()));
}

void CalculationTaskPage::attachMonitor(QWidget *widget)
{
    if (!widget) return;
    emptyChart_->hide();
    curveLayout_->addWidget(widget);
    widget->show();
}

void CalculationTaskPage::appendLog(const QString &message)
{
    log_->appendPlainText(QDateTime::currentDateTime().toString(QStringLiteral("[HH:mm:ss] ")) + message);
}

QVariantMap CalculationTaskPage::layoutState() const
{
    return {{QStringLiteral("settingsRatio"), splitRatio(mainSplitter_)},
        {QStringLiteral("plotRatio"), splitRatio(rightSplitter_)},
        {QStringLiteral("observablesRatio"), splitRatio(monitorSplitter_)}};
}

void CalculationTaskPage::restoreLayoutState(const QVariantMap &state)
{
    restoreRatio(mainSplitter_, state.value(QStringLiteral("settingsRatio"), 0.46).toDouble());
    restoreRatio(rightSplitter_, state.value(QStringLiteral("plotRatio"), 0.56).toDouble());
    if (multiple_) restoreRatio(monitorSplitter_, state.value(QStringLiteral("observablesRatio"), 0.32).toDouble());
}

void CalculationTaskPage::restoreDefaultLayout()
{
    mainSplitter_->setSizes({520, 670});
    rightSplitter_->setSizes({360, 270});
    if (multiple_) monitorSplitter_->setSizes({220, 480});
}

} // namespace aerosim
