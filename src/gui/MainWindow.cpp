#include "MainWindow.hpp"
#include "ProcessSelectorDialog.hpp"
#include <QDialog>
#include <QHeaderView>
#include <QMessageBox>
#include <QApplication>
#include <QStyleFactory>
#include <QPalette>
#include <thread>
#include <QInputDialog>
#include <algorithm>
#include <QFontDatabase>
#include <QScrollBar>
#include <QPainterPath>
#include <QMenu>
#include <QMessageBox>
#include <iostream>

void applyDarkTheme() {
    qApp->setStyle(QStyleFactory::create("Fusion"));

    QPalette darkPalette;
    darkPalette.setColor(QPalette::Window, QColor(26, 27, 38));
    darkPalette.setColor(QPalette::WindowText, QColor(192, 202, 245));
    darkPalette.setColor(QPalette::Base, QColor(22, 22, 30));
    darkPalette.setColor(QPalette::AlternateBase, QColor(26, 27, 38));
    darkPalette.setColor(QPalette::ToolTipBase, QColor(26, 27, 38));
    darkPalette.setColor(QPalette::ToolTipText, QColor(192, 202, 245));
    darkPalette.setColor(QPalette::Text, QColor(192, 202, 245));
    darkPalette.setColor(QPalette::Button, QColor(36, 40, 59));
    darkPalette.setColor(QPalette::ButtonText, QColor(192, 202, 245));
    darkPalette.setColor(QPalette::Link, QColor(122, 162, 247));
    darkPalette.setColor(QPalette::Highlight, QColor(122, 162, 247));
    darkPalette.setColor(QPalette::HighlightedText, QColor(26, 27, 38));

    qApp->setPalette(darkPalette);

    qApp->setStyleSheet(
        "QWidget { font-family: 'Segoe UI', 'Roboto', 'Arial'; font-size: 10pt; }"
        "QLineEdit { background-color: #24283b; border: 1px solid #414868; padding: 5px; border-radius: 4px; color: #c0caf5; }"
        "QPushButton { background-color: #414868; border: none; padding: 10px 20px; border-radius: 6px; color: #c0caf5; font-weight: bold; }"
        "QPushButton:hover { background-color: #7aa2f7; color: #1a1b26; }"
        "QPushButton:pressed { background-color: #bb9af7; }"
        "QTableWidget { background-color: #1a1b26; gridline-color: #414868; border: 1px solid #414868; border-radius: 4px; color: #c0caf5; }"
        "QHeaderView::section { background-color: #24283b; color: #c0caf5; padding: 5px; border: 1px solid #414868; }"
        "QStatusBar { background-color: #16161e; color: #565f89; }"
        "QTextEdit { background-color: #1a1b26; border: 1px solid #414868; border-radius: 4px; color: #c0caf5; font-family: 'Courier New', 'Consolas', 'monospace'; font-size: 10pt; }"
        "QTabWidget::pane { border: 1px solid #414868; background-color: #1a1b26; }"
        "QTabBar::tab { background-color: #24283b; border: 1px solid #414868; padding: 10px 20px; border-top-left-radius: 6px; border-top-right-radius: 6px; color: #565f89; }"
        "QTabBar::tab:selected { background-color: #1a1b26; color: #7aa2f7; border-bottom: 2px solid #7aa2f7; }"
        "QGraphicsView { background-color: #1a1b26; border: 1px solid #414868; }"
    );
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), 
      scanner(engine) 
{
    applyDarkTheme();
    config = Config::load();
    
    // Initialize Capstone
    if (cs_open(CS_ARCH_X86, CS_MODE_64, &capstoneHandle) != CS_ERR_OK) {
        capstoneHandle = 0;
    }

    setupUi();

    ui_uiRefreshTimer = new QTimer(this);
    connect(ui_uiRefreshTimer, &QTimer::timeout, this, &MainWindow::updateUiData);
    ui_uiRefreshTimer->start(100); // 10 FPS
}

MainWindow::~MainWindow() {
    if (capstoneHandle) {
        cs_close(&capstoneHandle);
    }
}

void MainWindow::setupUi() {
    ui_centralWidget = new QWidget(this);
    setCentralWidget(ui_centralWidget);
    ui_mainLayout = new QVBoxLayout(ui_centralWidget);

    // Top Bar (Process Info)
    auto *topBarLayout = new QHBoxLayout();
    ui_processInfoLabel = new QLabel("No process selected", this);
    ui_processInfoLabel->setStyleSheet("font-weight: bold; color: #7aa2f7; font-size: 12pt;");
    ui_selectProcessButton = new QPushButton("🔗 Select Process", this);
    topBarLayout->addWidget(ui_processInfoLabel);
    topBarLayout->addStretch();
    topBarLayout->addWidget(ui_selectProcessButton);
    ui_mainLayout->addLayout(topBarLayout);

    // Tabs
    ui_tabWidget = new QTabWidget(this);
    ui_mainLayout->addWidget(ui_tabWidget);

    // --- TAB 1: SCANNER ---
    ui_scannerTab = new QWidget();
    ui_scannerLayout = new QVBoxLayout(ui_scannerTab);
    
    auto *confRow = new QHBoxLayout();
    ui_valueTypeCombo = new QComboBox(ui_scannerTab);
    ui_valueTypeCombo->addItems({"Int8", "Int16", "Int32", "Int64", "UInt8", "UInt16", "UInt32", "UInt64", "Float32", "Float64", "Bool", "AOB", "String", "String16"});
    ui_valueTypeCombo->setCurrentIndex(2);
    
    ui_scanTypeCombo = new QComboBox(ui_scannerTab);
    ui_scanTypeCombo->addItems({"ExactValue", "NotEqual", "BiggerThan", "SmallerThan", "Between", "Increased", "IncreasedBy", "Decreased", "DecreasedBy", "Changed", "Unchanged"});
    
    confRow->addWidget(new QLabel("Type: ", ui_scannerTab));
    confRow->addWidget(ui_valueTypeCombo);
    confRow->addSpacing(20);
    confRow->addWidget(new QLabel("Mode: ", ui_scannerTab));
    confRow->addWidget(ui_scanTypeCombo);
    confRow->addStretch();
    ui_scannerLayout->addLayout(confRow);

    auto *searchRow = new QHBoxLayout();
    ui_searchValueInput = new QLineEdit(ui_scannerTab);
    ui_searchValueInput->setPlaceholderText("Enter value to scan...");
    ui_scanButton = new QPushButton("🔍 First Scan", ui_scannerTab);
    ui_resetButton = new QPushButton("🧹 Reset", ui_scannerTab);
    searchRow->addWidget(ui_searchValueInput);
    searchRow->addWidget(ui_scanButton);
    searchRow->addWidget(ui_resetButton);
    ui_scannerLayout->addLayout(searchRow);

    ui_resultsTable = new QTableWidget(0, 5, ui_scannerTab);
    ui_resultsTable->setHorizontalHeaderLabels({"Address", "Module", "+Offset", "Value", "Type"});
    ui_resultsTable->horizontalHeader()->setStretchLastSection(true);
    ui_resultsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui_resultsTable->setSelectionMode(QAbstractItemView::SingleSelection);
    ui_resultsTable->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui_resultsTable, &QWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        auto* item = ui_resultsTable->itemAt(pos);
        if (!item) return;
        
        int row = item->row();
        QString addrStr = ui_resultsTable->item(row, 0)->text();
        uintptr_t addr = addrStr.toULongLong(nullptr, 16);

        QMenu menu(this);
        menu.addAction("👁 Add to Watchlist", [this, addr]() {
            // Logic for adding to watchlist
            onAddWatchClicked(); 
        });
        menu.addAction("🛠 Go to in Debugger", [this, addr]() {
            currentDebugAddr = addr;
            ui_tabWidget->setCurrentIndex(2);
            refreshDisasmView();
            refreshHexView();
        });
        menu.addAction("📐 Dissect Structure", [this, addr]() {
            ui_dissectorBaseInput->setText(QString("0x%1").arg(addr, 0, 16).toUpper());
            ui_tabWidget->setCurrentIndex(7);
            onFillDissectorClicked();
        });
        menu.exec(ui_resultsTable->mapToGlobal(pos));
    });
    ui_scannerLayout->addWidget(ui_resultsTable);

    ui_tabWidget->addTab(ui_scannerTab, "🔍 Scanner");

    // --- TAB 2: MEMORY MAP ---
    ui_mapTab = new QWidget();
    ui_mapLayout = new QVBoxLayout(ui_mapTab);
    ui_mapText = new QTextEdit(ui_mapTab);
    ui_mapText->setReadOnly(true);
    ui_mapLayout->addWidget(ui_mapText);
    ui_tabWidget->addTab(ui_mapTab, "🗺 Map");

    // --- TAB 3: DEBUGGER ---
    ui_debugTab = new QWidget();
    ui_debugLayout = new QVBoxLayout(ui_debugTab);
    
    ui_gotoLayout = new QHBoxLayout();
    ui_gotoInput = new QLineEdit(ui_debugTab);
    ui_gotoInput->setPlaceholderText("Enter Address to Go To (e.g. 0x7FFFFFF or 123456)");
    ui_gotoButton = new QPushButton("🚀 Go to Address", ui_debugTab);
    ui_gotoLayout->addWidget(ui_gotoInput);
    ui_gotoLayout->addWidget(ui_gotoButton);
    ui_debugLayout->addLayout(ui_gotoLayout);

    ui_debugSplitter = new QSplitter(Qt::Vertical, ui_debugTab);

    ui_disasmText = new QTextEdit(ui_debugTab);
    ui_disasmText->setReadOnly(true);
    
    ui_hexTable = new QTableWidget(0, 17, ui_debugTab);
    QStringList hexHeaders; hexHeaders << "Address";
    for(int i=0; i<16; ++i) hexHeaders << QString("%1").arg(i, 2, 16, QChar('0')).toUpper();
    ui_hexTable->setHorizontalHeaderLabels(hexHeaders);

    ui_debugSplitter->addWidget(ui_disasmText);
    ui_debugSplitter->addWidget(ui_hexTable);
    ui_debugLayout->addWidget(ui_debugSplitter);
    ui_tabWidget->addTab(ui_debugTab, "🛠 Debug");

    // --- TAB 4: GRAPH (Blocks and Arrows) ---
    ui_graphTab = new QWidget();
    ui_graphLayout = new QVBoxLayout(ui_graphTab);
    
    ui_graphControlsLayout = new QHBoxLayout();
    ui_buildGraphButton = new QPushButton("🛠 Build Graph from Current Address", ui_graphTab);
    ui_traceGraphButton = new QPushButton("🎯 Start Live Trace", ui_graphTab);
    ui_graphSearchInput = new QLineEdit(ui_graphTab);
    ui_graphSearchInput->setPlaceholderText("Find address (0x...)");
    ui_graphSearchButton = new QPushButton("🔍 Find", ui_graphTab);
    
    ui_graphControlsLayout->addWidget(ui_buildGraphButton);
    ui_graphControlsLayout->addWidget(ui_traceGraphButton);
    ui_graphControlsLayout->addSpacing(40);
    ui_graphControlsLayout->addWidget(new QLabel("Search:"));
    ui_graphControlsLayout->addWidget(ui_graphSearchInput);
    ui_graphControlsLayout->addWidget(ui_graphSearchButton);
    ui_graphControlsLayout->addStretch();
    ui_graphLayout->addLayout(ui_graphControlsLayout);

    connect(ui_graphSearchButton, &QPushButton::clicked, this, &MainWindow::onSearchGraphClicked);
    connect(ui_graphSearchInput, &QLineEdit::returnPressed, this, &MainWindow::onSearchGraphClicked);

    ui_graphScene = new QGraphicsScene(ui_graphTab);
    ui_graphScene->setItemIndexMethod(QGraphicsScene::BspTreeIndex);
    ui_graphScene->setBspTreeDepth(10);
    
    ui_graphView = new ZoomableView(ui_graphScene, ui_graphTab);
    ui_graphView->setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    ui_graphView->setRenderHint(QPainter::SmoothPixmapTransform);
    ui_graphView->setOptimizationFlags(QGraphicsView::DontSavePainterState | QGraphicsView::DontAdjustForAntialiasing);
    ui_graphView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui_graphView, &QWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        auto* item = ui_graphView->itemAt(pos);
        if (item) {
            for (auto& arrow : activeArrows) {
                 if (arrow.wireItem == item || arrow.flowItem == item || arrow.arrowHead == item) {
                     QMenu menu(this);
                     menu.addAction("🔍 View Registers (RAX, RBX...)", [this, &arrow]() {
                         if (!arrow.hasData) {
                             QMessageBox::information(this, "Data Flow", "No execution data captured yet. Start Live Trace!");
                             return;
                         }
                         QString msg = QString("--- SNAPSHOT AT LAST EXECUTION ---\n"
                                               "RIP: 0x%1\nRAX: 0x%2\nRBX: 0x%3\nRCX: 0x%4\nRDX: 0x%5")
                             .arg(arrow.lastData.rip, 0, 16)
                             .arg(arrow.lastData.rax, 0, 16)
                             .arg(arrow.lastData.rbx, 0, 16)
                             .arg(arrow.lastData.rcx, 0, 16)
                             .arg(arrow.lastData.rdx, 0, 16);
                         QMessageBox::information(this, "Data Flow Trace", msg);
                     });
                     menu.addAction("Reset Hit Counter", [&arrow]() { arrow.hitCount = 0; });
                     menu.exec(ui_graphView->mapToGlobal(pos));
                     return;
                 }
            }
        }
    });
    ui_graphLayout->addWidget(ui_graphView);

    ui_tabWidget->addTab(ui_graphTab, "📈 Memory Graph");

    // --- TAB 5: WATCHLIST ---
    ui_watchTab = new QWidget();
    ui_watchLayout = new QVBoxLayout(ui_watchTab);
    auto* watchCtrl = new QHBoxLayout();
    ui_addWatchButton = new QPushButton("➕ Add Selected Address", ui_watchTab);
    watchCtrl->addWidget(ui_addWatchButton);
    watchCtrl->addStretch();
    ui_watchLayout->addLayout(watchCtrl);
    ui_watchTable = new QTableWidget(0, 4, ui_watchTab);
    ui_watchTable->setHorizontalHeaderLabels({"Address", "Type", "Value", "Description"});
    ui_watchTable->horizontalHeader()->setStretchLastSection(true);
    ui_watchTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui_watchTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui_watchLayout->addWidget(ui_watchTable);
    ui_tabWidget->addTab(ui_watchTab, "👁 Watchlist");

    // --- TAB 6: POINTER SCAN ---
    ui_ptrTab = new QWidget();
    ui_ptrLayout = new QVBoxLayout(ui_ptrTab);
    auto* ptrCtrl = new QHBoxLayout();
    ui_runPtrScanButton = new QPushButton("🔍 Run Pointer Scan on Selected", ui_ptrTab);
    ptrCtrl->addWidget(ui_runPtrScanButton);
    ptrCtrl->addStretch();
    ui_ptrLayout->addLayout(ptrCtrl);
    ui_ptrTable = new QTableWidget(0, 3, ui_ptrTab);
    ui_ptrTable->setHorizontalHeaderLabels({"Module", "Base Address", "Offsets"});
    ui_ptrTable->horizontalHeader()->setStretchLastSection(true);
    ui_ptrTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui_ptrLayout->addWidget(ui_ptrTable);
    ui_tabWidget->addTab(ui_ptrTab, "🔗 Pointers");

    // --- TAB 7: HW BREAKPOINTS ---
    ui_bpTab = new QWidget();
    ui_bpLayout = new QVBoxLayout(ui_bpTab);
    auto* bpCtrl = new QHBoxLayout();
    ui_setBpButton = new QPushButton("⚡ Set HW Breakpoint on Selected", ui_bpTab);
    bpCtrl->addWidget(ui_setBpButton);
    bpCtrl->addStretch();
    ui_bpLayout->addLayout(bpCtrl);
    ui_bpTable = new QTableWidget(0, 4, ui_bpTab);
    ui_bpTable->setHorizontalHeaderLabels({"Address", "Size", "Type", "Status"});
    ui_bpTable->horizontalHeader()->setStretchLastSection(true);
    ui_bpTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui_bpLayout->addWidget(ui_bpTable);
    ui_tabWidget->addTab(ui_bpTab, "🛑 Breakpoints");

    // --- TAB 8: STRUCTURE DISSECTOR ---
    ui_dissectorTab = new QWidget();
    ui_dissectorLayout = new QVBoxLayout(ui_dissectorTab);
    
    auto* disCtrl = new QHBoxLayout();
    ui_dissectorBaseInput = new QLineEdit(ui_dissectorTab);
    ui_dissectorBaseInput->setPlaceholderText("Base Address (e.g. 0x123456)");
    ui_dissectorAddButton = new QPushButton("➕ Add Field", ui_dissectorTab);
    ui_dissectorFillButton = new QPushButton("🧩 Auto-Fill", ui_dissectorTab);
    ui_dissectorClearButton = new QPushButton("🧹 Clear", ui_dissectorTab);
    
    disCtrl->addWidget(new QLabel("Base:", ui_dissectorTab));
    disCtrl->addWidget(ui_dissectorBaseInput);
    disCtrl->addWidget(ui_dissectorAddButton);
    disCtrl->addWidget(ui_dissectorFillButton);
    disCtrl->addWidget(ui_dissectorClearButton);
    disCtrl->addStretch();
    ui_dissectorLayout->addLayout(disCtrl);
    
    ui_dissectorTable = new QTableWidget(0, 5, ui_dissectorTab);
    ui_dissectorTable->setHorizontalHeaderLabels({"Offset", "Type", "Value", "Hex", "Description"});
    ui_dissectorTable->horizontalHeader()->setStretchLastSection(true);
    ui_dissectorTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui_dissectorLayout->addWidget(ui_dissectorTable);
    ui_tabWidget->addTab(ui_dissectorTab, "📐 Dissector");

    // Status Bar
    ui_statusBar = new QStatusBar(this);
    setStatusBar(ui_statusBar);
    ui_statusBar->showMessage("Ready.");

    // Connections
    connect(ui_scanButton, &QPushButton::clicked, this, &MainWindow::onScanClicked);
    connect(ui_resetButton, &QPushButton::clicked, this, &MainWindow::onResetClicked);
    connect(ui_selectProcessButton, &QPushButton::clicked, this, &MainWindow::onProcessSelectClicked);
    connect(ui_resultsTable, &QTableWidget::cellDoubleClicked, this, &MainWindow::onResultDoubleClicked);
    connect(ui_tabWidget, &QTabWidget::currentChanged, this, &MainWindow::onTabChanged);
    connect(ui_buildGraphButton, &QPushButton::clicked, this, &MainWindow::onBuildGraphClicked);
    
    connect(ui_traceGraphButton, &QPushButton::clicked, this, [this]() {
        if (!isTracing) {
            startLiveTrace();
        } else {
            isTracing = false;
            ui_traceGraphButton->setText("🎯 Start Live Trace");
            // The tracer thread will see isTracing=false, perform cleanup and exit.
        }
    });

    connect(ui_addWatchButton, &QPushButton::clicked, this, &MainWindow::onAddWatchClicked);
    connect(ui_runPtrScanButton, &QPushButton::clicked, this, &MainWindow::onRunPtrScanClicked);
    connect(ui_setBpButton, &QPushButton::clicked, this, &MainWindow::onSetBreakpointClicked);
    connect(ui_gotoButton, &QPushButton::clicked, this, &MainWindow::onGotoClicked);

    connect(ui_dissectorAddButton, &QPushButton::clicked, this, &MainWindow::onAddDissectorFieldClicked);
    connect(ui_dissectorClearButton, &QPushButton::clicked, this, &MainWindow::onClearDissectorClicked);
    connect(ui_dissectorFillButton, &QPushButton::clicked, this, &MainWindow::onFillDissectorClicked);
    
    // Connect combo box changes in the table (delegates would be better but let's do it simple first)
    connect(ui_dissectorTable, &QTableWidget::cellChanged, this, [this](int row, int col) {
        if (col == 1) { // Type changed
             // We'll handle this in refresh if we use combos as widgets, 
             // but if it's text we might need to parse.
             // For now let's use actual widgets.
        }
    });

    setWindowTitle("IxeRam");
    resize(1200, 850);
}

void MainWindow::onScanClicked() {
    if (engine.get_pid() == -1) {
        QMessageBox::warning(this, "Error", "Attach to a process first!");
        return;
    }
    if (scanner.is_scanning()) return;

    QString value = ui_searchValueInput->text();
    ValueType vt = static_cast<ValueType>(ui_valueTypeCombo->currentIndex());
    ScanType st = static_cast<ScanType>(ui_scanTypeCombo->currentIndex());

    scanner.set_scanning(true);
    std::string val = value.toStdString();
    bool isFirst = scanner.is_first_scan();

    std::thread([this, isFirst, vt, st, val]() {
        if (isFirst) scanner.initial_scan(vt, val);
        else scanner.next_scan(st, val);
    }).detach();

    ui_scanButton->setText("Searching...");
    ui_scanButton->setEnabled(false);
}

void MainWindow::onResetClicked() {
    scanner.clear_results();
    scanner.reset_first_scan();
    ui_resultsTable->setRowCount(0);
    ui_scanButton->setText("🔍 First Scan");
    ui_scanButton->setEnabled(true);
    ui_valueTypeCombo->setEnabled(true);
}

void MainWindow::updateUiData() {
    if (engine.get_pid() == -1) return;

    if (!scanner.is_scanning() && ui_scanButton->text() == "Searching...") {
        ui_scanButton->setText("🔍 Next Scan");
        ui_scanButton->setEnabled(true);
        ui_statusBar->showMessage(QString("Found %1 results.").arg(scanner.get_results().size()));
        refreshScannerTable();
    }

    if (scanner.is_scanning()) {
        ui_statusBar->showMessage(QString("Scanning... %1%").arg((int)(scanner.get_progress()*100)));
    }

    int tab = ui_tabWidget->currentIndex();
    
    // Animate Graph Arrows (Runners) & Handle Dragging
    // Must run even during tracing for visual feedback!
    if (tab == 3 && !activeArrows.empty()) {
        for (auto& arrow : activeArrows) {
            QPen p = arrow.flowItem->pen();
            if (p.style() == Qt::CustomDashLine) {
                p.setDashOffset(p.dashOffset() - 4.0); 
                arrow.flowItem->setPen(p);
            }

            // Highlight logic: Combine Selection Glow with Tracer Heatmap
            QColor currentBase = QColor("#414868"); // Visible subtle gray-blue for inactive paths
            if (arrow.hitCount > 0) {
                 currentBase = QColor("#9ece6a"); // Green
                 if (arrow.hitCount > 100) currentBase = QColor("#ff9e64"); // Orange
                 if (arrow.hitCount > 1000) currentBase = QColor("#f7768e"); // Red
            }

            if (arrow.flowItem->isSelected() || arrow.wireItem->isSelected()) {
                arrow.wireItem->setPen(QPen(QColor("#bb9af7"), 9, Qt::SolidLine)); // Selection Glow
                arrow.flowItem->setPen(QPen(currentBase, 4, Qt::SolidLine));       // Thick Heat Flow
            } else {
                arrow.wireItem->setPen(QPen(QColor("#1a1b26"), 6, Qt::SolidLine)); // Dark background cable
                arrow.flowItem->setPen(QPen(currentBase, 2, arrow.hitCount > 0 ? Qt::SolidLine : Qt::CustomDashLine));
                if (arrow.hitCount == 0) {
                    QPen p = arrow.flowItem->pen();
                    p.setDashPattern({5, 10});
                    arrow.flowItem->setPen(p);
                }
            }
            
            // Re-calculate path points based on current UI positions of nodes
            QPointF start = arrow.sourceNode->mapToScene(arrow.sourceNode->rect().center().x(), arrow.sourceNode->rect().bottom());
            QPointF end = arrow.targetNode->mapToScene(arrow.targetNode->rect().center().x(), arrow.targetNode->rect().top());
            
            QPainterPath path;
            path.moveTo(start);
            path.cubicTo(start.x(), start.y() + 40, end.x(), end.y() - 40, end.x(), end.y());
            arrow.wireItem->setPath(path);
            arrow.flowItem->setPath(path);
            
            // Adjust Arrow head tip rotation/position
            QPolygonF head;
            head << end << QPointF(end.x() - 6, end.y() - 12) << QPointF(end.x() + 6, end.y() - 12);
            arrow.arrowHead->setPolygon(head);
            arrow.arrowHead->setBrush(QBrush(currentBase));
            arrow.arrowHead->setPen(QPen(currentBase));
            
            // Adjust Info Text
            if (arrow.infoText) {
                arrow.infoText->setPos(path.pointAtPercent(0.5) + QPointF(5, -10));
                arrow.infoText->setDefaultTextColor(currentBase);
            }
        }
    }

    if (isTracing) return; // Only block heavy memory-reading tables

    if (tab == 0) updateScannerLiveValues();
    else if (tab == 1) refreshMemoryMap();
    else if (tab == 2) {
        refreshDisasmView();
        refreshHexView();
    }
    else if (tab == 4) refreshWatchlist();
    else if (tab == 5) refreshPtrScanTable();
    else if (tab == 6) refreshBpHitsTable();
    else if (tab == 7) refreshDissectorView();
}

void MainWindow::refreshScannerTable() {
    const auto& raw = scanner.get_results();
    auto regions = engine.update_maps();
    std::sort(regions.begin(), regions.end(), [](const auto &a, const auto &b){ return a.start < b.start; });

    ui_resultsTable->setUpdatesEnabled(false); // HUGE PERFORMANCE BOOST
    size_t count = std::min(raw.size(), (size_t)500);
    ui_resultsTable->setRowCount(count);

    for (size_t i = 0; i < count; ++i) {
        uintptr_t addr = raw[i].address;
        std::string mod_name = "[anon]";
        uintptr_t offset = addr;

        auto it = std::upper_bound(regions.begin(), regions.end(), addr, [](uintptr_t a, const auto &reg){ return a < reg.start; });
        if (it != regions.begin()) {
            const auto &reg = *(--it);
            if (addr >= reg.start && addr < reg.end) {
                size_t last_slash = reg.pathname.find_last_of('/');
                mod_name = (last_slash != std::string::npos) ? reg.pathname.substr(last_slash + 1) : reg.pathname;
                if (mod_name.empty()) mod_name = "[anon]";
                offset = addr - reg.start;
            }
        }

        ui_resultsTable->setItem(i, 0, new QTableWidgetItem(QString("0x%1").arg(addr, 12, 16, QChar('0')).toUpper()));
        ui_resultsTable->setItem(i, 1, new QTableWidgetItem(QString::fromStdString(mod_name)));
        ui_resultsTable->setItem(i, 2, new QTableWidgetItem(QString("+0x%1").arg(offset, 0, 16).toUpper()));
        ui_resultsTable->setItem(i, 3, new QTableWidgetItem(QString::fromStdString(scanner.read_value_str(addr))));
        ui_resultsTable->setItem(i, 4, new QTableWidgetItem(QString::fromStdString(valueTypeName(scanner.get_value_type()))));
    }
    ui_resultsTable->setUpdatesEnabled(true);
}

void MainWindow::updateScannerLiveValues() {
    if (scanner.is_scanning() || ui_resultsTable->rowCount() == 0) return;
    
    // Auto-update values in the main scanner table directly without redrawing rows
    for (int i = 0; i < ui_resultsTable->rowCount(); ++i) {
        auto* addrItem = ui_resultsTable->item(i, 0);
        auto* valItem = ui_resultsTable->item(i, 3);
        if (addrItem && valItem) {
            uintptr_t addr = addrItem->text().toULongLong(nullptr, 16);
            std::string currentVal = scanner.read_value_str(addr);
            QString newText = QString::fromStdString(currentVal);
            if (valItem->text() != newText) {
                valItem->setText(newText);
            }
        }
    }
}

void MainWindow::onResultDoubleClicked(int row, int column) {
    QString addrStr = ui_resultsTable->item(row, 0)->text();
    uintptr_t addr = addrStr.toULongLong(nullptr, 16);
    currentDebugAddr = addr;

    bool ok;
    QString newVal = QInputDialog::getText(this, "Patch Value", QString("Addr: %1").arg(addrStr), QLineEdit::Normal, ui_resultsTable->item(row, 3)->text(), &ok);
    
    if (ok && !newVal.isEmpty()) {
        if (scanner.write_value(addr, newVal.toStdString(), scanner.get_value_type())) {
            ui_statusBar->showMessage("Value updated.");
            refreshScannerTable();
        }
    }
}

void MainWindow::refreshMemoryMap() {
    static size_t lastRegionCount = 0;
    auto regions = engine.update_maps();
    if (regions.size() == lastRegionCount) return; // Prevent selection clearing if map didn't change wildly
    lastRegionCount = regions.size();
    
    int scrollPos = ui_mapText->verticalScrollBar()->value();
    QString html = "<html><body style='font-family:monospace;'>";
    html += "<b><span style='color:#7aa2f7;'>START           END             SIZE       PERMS  MODULE</span></b><br>";
    for (const auto &reg : regions) {
        QString start = QString("0x%1").arg(reg.start, 12, 16, QChar('0')).toUpper();
        QString end = QString("0x%1").arg(reg.end, 12, 16, QChar('0')).toUpper();
        QString size = QString("%1 KB").arg((reg.end - reg.start)/1024).leftJustified(10);
        QString color = (reg.permissions.find('x') != std::string::npos) ? "#bb9af7" : "#c0caf5";
        html += QString("<span style='color:#565f89;'>%1 - %2  </span>").arg(start, end);
        html += QString("<span style='color:#9ece6a;'>%1</span> ").arg(size);
        html += QString("<span style='color:#f7768e;'>[%1]</span> ").arg(QString::fromStdString(reg.permissions));
        html += QString("<span style='color:%1;'>%2</span><br>").arg(color, QString::fromStdString(reg.pathname));
    }
    html += "</body></html>";
    ui_mapText->setHtml(html);
    ui_mapText->verticalScrollBar()->setValue(scrollPos);
}

void MainWindow::refreshDisasmView() {
    static uintptr_t lastDisasmAddr = 0;
    if (!capstoneHandle || !currentDebugAddr) return;
    if (currentDebugAddr == lastDisasmAddr) return; // Fix text selection clearing loop
    lastDisasmAddr = currentDebugAddr;
    
    int scrollPos = ui_disasmText->verticalScrollBar()->value();
    std::vector<uint8_t> buf(256);
    if (!engine.read_memory(currentDebugAddr, buf.data(), buf.size())) {
        ui_disasmText->setHtml(QString("<div style='color:#f7768e; padding:10px;'>Invalid or unreadable address: 0x%1</div>").arg(currentDebugAddr, 0, 16));
        return;
    }
    cs_insn *insn;
    size_t count = cs_disasm(capstoneHandle, buf.data(), buf.size(), currentDebugAddr, 0, &insn);
    if (count > 0) {
        QString html = "<html><body style='font-family:monospace;'>";
        for (size_t i = 0; i < std::min(count, (size_t)35); ++i) {
            QString addrStr = QString(".text:%1").arg(insn[i].address, 12, 16, QChar('0')).toUpper();
            QString mnemonic = QString(insn[i].mnemonic).leftJustified(8);
            QString ops = QString(insn[i].op_str);
            bool isSelection = (insn[i].address == currentDebugAddr);
            QString style = isSelection ? "background-color:#2e3c64; color:#ffffff;" : "color:#c0caf5;";
            html += QString("<div style='%1'><span style='color:#565f89;'>%2</span>  <span style='font-weight:bold; color:#7aa2f7;'>%3</span> %4</div>").arg(style, addrStr, mnemonic, ops);
        }
        html += "</body></html>";
        ui_disasmText->setHtml(html);
        ui_disasmText->verticalScrollBar()->setValue(scrollPos);
        cs_free(insn, count);
    }
}

void MainWindow::refreshHexView() {
    if (!currentDebugAddr) return;
    
    std::vector<uint8_t> buf(16 * 10);
    if (!engine.read_memory(currentDebugAddr, buf.data(), buf.size())) {
        ui_hexTable->setRowCount(0); // Clear to show it's invalid
        return;
    }
    
    ui_hexTable->setUpdatesEnabled(false);
    if (ui_hexTable->rowCount() != 10) ui_hexTable->setRowCount(10);
    
    for (int i = 0; i < 10; ++i) {
        if (!ui_hexTable->item(i, 0)) ui_hexTable->setItem(i, 0, new QTableWidgetItem());
        ui_hexTable->item(i, 0)->setText(QString("0x%1").arg(currentDebugAddr + i*16, 8, 16, QChar('0')).toUpper());
        
        for (int j = 0; j < 16; ++j) {
            uint8_t b = buf[i*16 + j];
            if (!ui_hexTable->item(i, j + 1)) ui_hexTable->setItem(i, j + 1, new QTableWidgetItem());
            
            auto *item = ui_hexTable->item(i, j + 1);
            item->setText(QString("%1").arg(b, 2, 16, QChar('0')).toUpper());
            
            if (b == 0) item->setForeground(QColor("#414868"));
            else if (b >= 0x20 && b < 0x7F) item->setForeground(QColor("#9ece6a"));
            else item->setForeground(QColor("#c0caf5"));
        }
    }
    ui_hexTable->setUpdatesEnabled(true);
}

void MainWindow::onBuildGraphClicked() {
    if (currentDebugAddr == 0) {
        QMessageBox::warning(this, "Error", "No address selected! Double-click a scan result or use 'Go To Address' in the Debug tab first.");
        return;
    }
    uintptr_t addr = currentDebugAddr;
    
    auto regions = engine.update_maps();
    bool isExec = false;
    for(const auto& reg : regions) {
        if(addr >= reg.start && addr < reg.end) {
            if(reg.permissions.find('x') != std::string::npos) isExec = true;
            break;
        }
    }
    
    if(!isExec) {
        QMessageBox::warning(this, "Memory Graph", 
            "Вы выбрали адрес ДАННЫХ, а не КОДА.\n\n"
            "Call Graph строится только из инструкций программы (executable segment).\n"
            "Чтобы узнать, какой код общается с этими данными:\n"
            "1. Зайдите во вкладку Breakpoints.\n"
            "2. Поставьте аппаратный брейкпоинт (HW Breakpoint).\n"
            "3. Постройте Граф из пойманного адреса (RIP).");
        return;
    }
    
    buildCallGraph(addr);
    ui_tabWidget->setCurrentIndex(3); // Switch to Graph tab
}

void MainWindow::buildCallGraph(uintptr_t root) {
    ui_graphScene->clear();
    graphNodes.clear();
    activeArrows.clear();

    struct NodeInfo {
        uintptr_t addr;
        int level;
        int x_pos;
    };

    std::vector<NodeInfo> queue;
    queue.push_back({root, 0, 0});
    
    std::map<int, int> levelWidths; // level -> current count at this level
    
    // Limits
    const int MAX_DEPTH = 3;
    const int MAX_NODES = 25;
    
    int nodeCount = 0;
    while(!queue.empty() && nodeCount < MAX_NODES) {
        NodeInfo current = queue.front();
        queue.erase(queue.begin());

        if (graphNodes.count(current.addr)) continue;

        // Fetch disasm for this node
        std::vector<uint8_t> buf(128);
        engine.read_memory(current.addr, buf.data(), 128);
        cs_insn *insn;
        size_t count = cs_disasm(capstoneHandle, buf.data(), buf.size(), current.addr, 0, &insn);
        
        GraphNode node;
        node.addr = current.addr;
        
        QString html = "<div style='font-family:monospace; line-height:1.1; font-size:9pt;'>";
        html += QString("<div style='background-color:#16161e; color:#7aa2f7; padding:4px; font-weight:bold;'>loc_%1:</div>")
                .arg(current.addr, 8, 16, QChar('0')).toUpper();
        
        int linesToShow = std::min((int)count, 8);
        for(int i = 0; i < linesToShow; ++i) {
            QString addrStr = QString::number(insn[i].address, 16).toUpper().rightJustified(8, '0');
            QString mnemonic = QString(insn[i].mnemonic).leftJustified(6);
            QString ops = QString(insn[i].op_str);
            
            // Highlight calls/jmps
            QString mColor = "#c0caf5";
            if (mnemonic.startsWith("j") || mnemonic == "call") mColor = "#f7768e"; // red
            else if (mnemonic == "mov" || mnemonic == "lea") mColor = "#7dcfff"; // cyan
            
            html += QString("<div style='padding:2px;'><span style='color:#565f89;'>%1</span> <span style='color:%2; font-weight:bold;'>%3</span> <span style='color:#9ece6a;'>%4</span></div>")
                    .arg(addrStr, mColor, mnemonic, ops);
        }
        html += "</div>";
        
        auto* text = new QGraphicsTextItem();
        text->setHtml(html);
        QRectF br = text->boundingRect();
        
        // Create rectangle matching text size
        int nodeW = std::max(220, (int)br.width() + 10);
        int nodeH = br.height() + 10;
        int x = levelWidths[current.level] * (nodeW + 80);
        int y = current.level * 200;
        levelWidths[current.level]++;
        
        auto* rect = ui_graphScene->addRect(x, y, nodeW, nodeH, QPen(QColor("#7aa2f7")), QBrush(QColor("#24283b")));
        rect->setFlag(QGraphicsItem::ItemIsMovable, true);
        rect->setFlag(QGraphicsItem::ItemIsSelectable, true);
        rect->setCursor(Qt::SizeAllCursor);
        
        text->setParentItem(rect);
        text->setPos(x + 5, y + 5);
        
        node.rect = rect;

        // Parse callees (jumps, calls, conditional branches)
        for(size_t i=0; i<count; ++i) {
            std::string mn = insn[i].mnemonic;
            QString ops = QString(insn[i].op_str);
            
            // All types of jumps (j*) or calls to absolute addresses
            if ((mn[0] == 'j' || mn == "call") && ops.startsWith("0x")) {
                bool ok = false;
                uintptr_t target = ops.toULongLong(&ok, 16);
                if (ok && target > 0 && current.level < MAX_DEPTH) {
                    node.callees.push_back(target);
                    // Add to queue to traverse deeper
                    queue.push_back({target, current.level + 1, 0});
                }
            }
        }
        
        if (count > 0) cs_free(insn, count);
        
        graphNodes[current.addr] = node;
        nodeCount++;
    }
    
    // Draw Arrows with Bezier curves and Animated dashed lines
    for (auto const& [addr, node] : graphNodes) {
        for (uintptr_t calleeAddr : node.callees) {
            if (graphNodes.count(calleeAddr)) {
                auto& targetNode = graphNodes[calleeAddr];
                QPainterPath path;
                QPointF start = node.rect->mapToScene(node.rect->rect().center().x(), node.rect->rect().bottom());
                QPointF end = targetNode.rect->mapToScene(targetNode.rect->rect().center().x(), targetNode.rect->rect().top());
                
                path.moveTo(start);
                path.cubicTo(start.x(), start.y() + 40, end.x(), end.y() - 40, end.x(), end.y());
                
                // Background Cable
                QPen wirePen(QColor("#1a1b26"), 6, Qt::SolidLine);
                auto* wirePath = ui_graphScene->addPath(path, wirePen);
                wirePath->setZValue(-2);
                wirePath->setFlag(QGraphicsItem::ItemIsSelectable, true);
                wirePath->setCursor(Qt::PointingHandCursor);
                
                // Flowing Energy
                QPen flowPen(QColor("#7dcfff"), 2, Qt::CustomDashLine);
                flowPen.setDashPattern({5, 10});
                auto* flowPath = ui_graphScene->addPath(path, flowPen);
                flowPath->setZValue(-1);
                flowPath->setFlag(QGraphicsItem::ItemIsSelectable, true);
                flowPath->setCursor(Qt::PointingHandCursor);
                
                // Add Arrow Head (Triangle) matching flow
                auto* aHead = ui_graphScene->addPolygon(QPolygonF(), QPen(QColor("#7dcfff")), QBrush(QColor("#7dcfff")));
                aHead->setZValue(-1);
                aHead->setFlag(QGraphicsItem::ItemIsSelectable, true);
                aHead->setCursor(Qt::PointingHandCursor);
                
                // Add Connection Label
                auto* aText = ui_graphScene->addText("Data Flow", QFont("Segoe UI", 8, QFont::Bold));
                aText->setDefaultTextColor(QColor("#565f89"));
                aText->setFlag(QGraphicsItem::ItemIsSelectable, true);
                
                GraphArrow gArr;
                gArr.wireItem = wirePath;
                gArr.flowItem = flowPath;
                gArr.arrowHead = aHead;
                gArr.infoText = aText;
                gArr.sourceNode = node.rect;
                gArr.targetNode = targetNode.rect;
                gArr.hitCount = 0;
                gArr.hasData = false;
                activeArrows.push_back(gArr);
            }
        }
    }
}

void MainWindow::onTabChanged(int index) {
    if (index == 2 && currentDebugAddr == 0 && ui_resultsTable->rowCount() > 0) {
        QString addrStr = ui_resultsTable->item(0, 0)->text();
        currentDebugAddr = addrStr.toULongLong(nullptr, 16);
    }
}

void MainWindow::onProcessSelectClicked() {
    ProcessSelectorDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        pid_t pid = dialog.getSelectedPid();
        if (pid != -1) {
            if (engine.attach(pid)) {
                ui_processInfoLabel->setText(QString("🟢 PID: %1").arg(pid));
                onResetClicked();
                ui_statusBar->showMessage("Successfully attached.");
            }
        }
    }
}

void MainWindow::onAddWatchClicked() {
    if (currentDebugAddr == 0) {
        QMessageBox::warning(this, "Watchlist", "Please select an address first.");
        return;
    }
    bool ok;
    QString desc = QInputDialog::getText(this, "Watchlist", "Enter description:", QLineEdit::Normal, "", &ok);
    if (ok) {
        int row = ui_watchTable->rowCount();
        ui_watchTable->insertRow(row);
        ui_watchTable->setItem(row, 0, new QTableWidgetItem(QString("0x%1").arg(currentDebugAddr, 0, 16).toUpper()));
        ui_watchTable->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(valueTypeName(scanner.get_value_type()))));
        ui_watchTable->setItem(row, 2, new QTableWidgetItem("?"));
        ui_watchTable->setItem(row, 3, new QTableWidgetItem(desc));
        ui_statusBar->showMessage(QString("Added 0x%1 to watchlist").arg(currentDebugAddr, 0, 16));
        ui_tabWidget->setCurrentIndex(4);
    }
}

void MainWindow::onGotoClicked() {
    QString text = ui_gotoInput->text().trimmed();
    if (text.isEmpty()) return;
    
    uintptr_t addr = 0;
    
    // Parse Module + Offset (e.g. client.dll+0x1000)
    if (text.contains("+")) {
        QStringList parts = text.split("+");
        QString moduleName = parts[0].trimmed();
        QString offsetStr = parts[1].trimmed();
        
        uintptr_t offset = 0;
        if (offsetStr.startsWith("0x", Qt::CaseInsensitive)) offset = offsetStr.toULongLong(nullptr, 16);
        else offset = offsetStr.toULongLong(nullptr, 16); // Default to hex for offsets
        
        auto regions = engine.update_maps();
        for(const auto& reg : regions) {
            QString path = QString::fromStdString(reg.pathname);
            if(path.endsWith(moduleName, Qt::CaseInsensitive) || path.contains(moduleName, Qt::CaseInsensitive)) {
                addr = reg.start + offset;
                break;
            }
        }
    } else {
        bool ok = false;
        if (text.startsWith("0x", Qt::CaseInsensitive)) {
            addr = text.toULongLong(&ok, 16);
        } else {
            addr = text.toULongLong(&ok, 16); // Try hex first for reverse engineering
        }
        
        // If it's a small address (less than 1MB), assume it's an offset from the main module
        if (ok && addr < 0x100000) {
            auto regions = engine.update_maps();
            uintptr_t mainBase = 0;
            for(const auto& reg : regions) {
                // The first executable region with a filename is usually the main module
                if (reg.is_readable() && !reg.pathname.empty() && 
                    (reg.permissions.find('x') != std::string::npos || mainBase == 0)) {
                    mainBase = reg.start;
                    break;
                }
            }
            if (mainBase > 0) {
                uintptr_t offset = addr;
                addr = mainBase + offset;
                ui_statusBar->showMessage(QString("Auto-resolved offset: 0x%1 -> 0x%2").arg(offset, 0, 16).arg(addr, 0, 16));
            }
        }
    }
    
    if (addr > 0) {
        currentDebugAddr = addr;
        // Force refresh by zeroing cache
        refreshDisasmView();
        refreshHexView();
        ui_statusBar->showMessage(QString("Jumped to 0x%1").arg(addr, 0, 16));
        ui_tabWidget->setCurrentIndex(2); // Auto switch to debug tab if not already on it
    } else {
        QMessageBox::warning(this, "Error", "Failed to resolve address or module offset!");
    }
}

void MainWindow::onRunPtrScanClicked() {
    if (currentDebugAddr == 0) {
        QMessageBox::warning(this, "Error", "Select an address from the Scanner table first!");
        return;
    }
    ui_statusBar->showMessage("Running pointer scan... This may take a moment.");
    ui_runPtrScanButton->setText("Scanning...");
    ui_runPtrScanButton->setEnabled(false);
    
    uintptr_t target_addr = currentDebugAddr;
    
    // Run pointer scan asynchronously
    std::thread([this, target_addr]() {
        // Max depth 2, max offset 1024
        auto ptrs = scanner.find_pointers(target_addr, 2, 1024);
        
        QMetaObject::invokeMethod(this, [this, ptrs]() {
            ui_ptrTable->setUpdatesEnabled(false);
            ui_ptrTable->setRowCount(ptrs.size());
            
            for (size_t i = 0; i < ptrs.size(); ++i) {
                const auto& p = ptrs[i];
                QString offsets;
                for (auto o : p.offsets) {
                    offsets += QString("%10x%2 ")
                                .arg(o >= 0 ? "+" : "-")
                                .arg(std::abs(o), 0, 16);
                }
                
                ui_ptrTable->setItem(i, 0, new QTableWidgetItem(QString::fromStdString(p.module_name)));
                ui_ptrTable->setItem(i, 1, new QTableWidgetItem(QString("0x%1").arg(p.base_module_addr, 12, 16, QChar('0')).toUpper()));
                ui_ptrTable->setItem(i, 2, new QTableWidgetItem(offsets.trimmed()));
            }
            ui_ptrTable->setUpdatesEnabled(true);
            ui_runPtrScanButton->setText("🔍 Run Pointer Scan on Selected");
            ui_runPtrScanButton->setEnabled(true);
            ui_statusBar->showMessage(QString("Pointer scan finished. Found %1 paths.").arg(ptrs.size()));
        });
    }).detach();
}

void MainWindow::onSetBreakpointClicked() {
    if (currentDebugAddr == 0) return;
    ui_statusBar->showMessage(QString("Set HW Breakpoint on 0x%1").arg(currentDebugAddr, 0, 16));
    // TUI calls engine.set_hw_breakpoint(...)
    ui_tabWidget->setCurrentIndex(6);
}

void MainWindow::refreshWatchlist() {
    if (ui_watchTable->rowCount() == 0) return;
    
    // Live update exactly like the Scanner view
    for (int i = 0; i < ui_watchTable->rowCount(); ++i) {
        auto* addrItem = ui_watchTable->item(i, 0);
        auto* valItem = ui_watchTable->item(i, 2); // Column 2 is "Value"
        if (addrItem && valItem) {
            uintptr_t addr = addrItem->text().toULongLong(nullptr, 16);
            std::string currentVal = scanner.read_value_str(addr);
            QString newText = QString::fromStdString(currentVal);
            if (valItem->text() != newText) {
                valItem->setText(newText);
                valItem->setForeground(QColor("#bb9af7")); // Highlight changed
            }
        }
    }
}

void MainWindow::refreshPtrScanTable() {
    // Refresh pointer scan table data
}

void MainWindow::refreshBpHitsTable() {
    // Refresh breakpoint hits (or HW breakpoints list)
}

void MainWindow::startLiveTrace() {
    if (graphNodes.empty()) return;
    
    isTracing = true;
    ui_traceGraphButton->setText("🛑 Stop Trace");
    
    if (tracerThread.joinable()) tracerThread.join();
    
    // Safety: Clear previous records
    engine.access_records.clear();

    // Run tracer in background
    tracerThread = std::thread([this]() {
        uintptr_t hit_addr = 0;
        
        // CRITICAL (Linux Ptrace Affinity): 
        // WE MUST ATTACH in this thread to be the official tracer.
        if (!engine.attach_ptrace()) {
             QString err = QString("Tracer failed: %1 (errno %2)").arg(strerror(errno)).arg(errno);
             QMetaObject::invokeMethod(this, [this, err]() {
                 ui_statusBar->showMessage(err, 5000);
                 ui_traceGraphButton->setText("🎯 Start Live Trace");
             });
             isTracing = false;
             return;
        }
        
        std::cout << "[Tracer] Attached and stopped. Setting breakpoints...\n";
        
        for (auto const& [addr, node] : graphNodes) {
            engine.set_breakpoint(addr);
        }
        
        // Start waiting cycle
        engine.resume_process();
        
        auto lastUiUpdate = std::chrono::steady_clock::now();
        std::map<uintptr_t, std::pair<int, AccessRecord>> pendingHits;

        while (isTracing) {
            if (engine.wait_breakpoint(hit_addr, 50)) {
                AccessRecord record;
                bool hasRecord = false;
                if (!engine.access_records.empty()) {
                    record = engine.access_records.back();
                    hasRecord = true;
                }
                
                // Buffer the hit instead of immediate UI call
                pendingHits[hit_addr].first++;
                if (hasRecord) {
                    record.target_addr = 0;
                    
                    // --- DYNAMIC GRAPH EXPANSION LOGIC ---
                    // Read the instruction at the hit address (need to restore original byte temporarily)
                    uint8_t orig_byte = 0;
                    if (engine.read_memory(hit_addr, &orig_byte, 1)) {
                         // We have the address. Let's see if it's a branch instruction
                         std::vector<uint8_t> insn_buf(15); // Max x86 insn size
                         engine.read_memory(hit_addr, insn_buf.data(), 15);
                         
                         cs_insn *insn;
                         size_t count = cs_disasm(capstoneHandle, insn_buf.data(), 15, hit_addr, 1, &insn);
                         if (count > 0) {
                             QString qMn = QString(insn[0].mnemonic);
                             QString ops = QString(insn[0].op_str);
                             
                             // If it's a CALL or JMP with an absolute address
                             if ((qMn == "call" || qMn.startsWith("j")) && ops.startsWith("0x")) {
                                 bool ok;
                                 uintptr_t target = ops.toULongLong(&ok, 16);
                                 if (ok) record.target_addr = target;
                             }
                             cs_free(insn, count);
                         }
                    }
                    pendingHits[hit_addr].second = record;
                }
                
                // New logic: step_over handles removing/restoring BP internally
                engine.step_over(hit_addr); 
                
                // Resume full-speed execution
                engine.resume_process();
            } else if (engine.get_pid() == -1) {
                // Target process gone
                isTracing = false;
                QMetaObject::invokeMethod(this, [this]() {
                    ui_statusBar->showMessage("Tracer: Target process exited.", 5000);
                    ui_traceGraphButton->setText("🎯 Start Live Trace");
                });
            }

            // Periodically push buffered hits to UI (30 FPS)
            auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastUiUpdate).count() > 33) {
                if (!pendingHits.empty()) {
                    auto hitsCopy = pendingHits;
                    pendingHits.clear();
                    
                    QMetaObject::invokeMethod(this, [this, hitsCopy]() {
                        for (auto const& [addr, data] : hitsCopy) {
                            if (graphNodes.count(addr)) {
                                 auto& targetNode = graphNodes[addr];
                                 targetNode.rect->setPen(QPen(QColor("#9ece6a"), 4));
                                 
                                 for (auto& arrow : activeArrows) {
                                     if (arrow.targetNode == targetNode.rect) {
                                         arrow.hitCount += data.first;
                                         arrow.lastData = data.second;
                                         arrow.hasData = true;

                                         QColor heatColor = QColor("#9ece6a");
                                         if (arrow.hitCount > 100) heatColor = QColor("#ff9e64");
                                         if (arrow.hitCount > 1000) heatColor = QColor("#f7768e");
                                         
                                         arrow.flowItem->setPen(QPen(heatColor, 4, Qt::SolidLine));
                                         if (arrow.infoText) {
                                             arrow.infoText->setPlainText(QString("[%1] RAX: 0x%2").arg(arrow.hitCount).arg(arrow.lastData.rax, 0, 16).toUpper());
                                             arrow.infoText->setDefaultTextColor(heatColor);
                                         }
                                     }
                                 }
                                 
                                 // --- DYNAMIC EXPANSION ---
                                 if (data.second.target_addr > 0 && !graphNodes.count(data.second.target_addr)) {
                                     addNodeToGraph(data.second.target_addr, addr);
                                 }
                            }
                        }
                    });
                }
                lastUiUpdate = now;
            }
        }
        
        // --- CLEANUP (Still in Tracer Thread context) ---
        std::cout << "[Tracer] Stopping and cleaning up breakpoints...\n";
        engine.clear_breakpoints();
        engine.resume_process();
        engine.detach_ptrace(); // Hand over control back
        std::cout << "[Tracer] Clean exit. Game resumed.\n";
    });
}

void MainWindow::onAddDissectorFieldClicked() {
    int nextOffset = 0;
    if (!dissectorFields.empty()) {
        const auto& last = dissectorFields.back();
        int size = 4;
        switch(last.type) {
            case DissectorType::Int8: case DissectorType::UInt8: size = 1; break;
            case DissectorType::Int16: case DissectorType::UInt16: size = 2; break;
            case DissectorType::Int32: case DissectorType::UInt32: case DissectorType::Float: size = 4; break;
            case DissectorType::Int64: case DissectorType::UInt64: case DissectorType::Double: case DissectorType::Pointer: size = 8; break;
            case DissectorType::String: size = 16; break;
        }
        nextOffset = last.offset + size;
    }
    dissectorFields.push_back({nextOffset, DissectorType::Int32, "Field " + QString::number(dissectorFields.size())});
    
    int row = ui_dissectorTable->rowCount();
    ui_dissectorTable->insertRow(row);
    ui_dissectorTable->setItem(row, 0, new QTableWidgetItem(QString("+0x%1").arg(nextOffset, 0, 16).toUpper()));
    
    QComboBox* combo = new QComboBox();
    combo->addItems({"Int8", "UInt8", "Int16", "UInt16", "Int32", "UInt32", "Int64", "UInt64", "Float", "Double", "Pointer", "String"});
    combo->setCurrentIndex(4); // Int32
    ui_dissectorTable->setCellWidget(row, 1, combo);
    
    ui_dissectorTable->setItem(row, 2, new QTableWidgetItem("0"));
    ui_dissectorTable->setItem(row, 3, new QTableWidgetItem("00 00 00 00"));
    ui_dissectorTable->setItem(row, 4, new QTableWidgetItem(dissectorFields.back().description));
    
    connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), [this, row](int index){
        dissectorFields[row].type = static_cast<DissectorType>(index);
    });
}

void MainWindow::onClearDissectorClicked() {
    dissectorFields.clear();
    ui_dissectorTable->setRowCount(0);
}

void MainWindow::onFillDissectorClicked() {
    onClearDissectorClicked();
    for (int i = 0; i < 16; ++i) {
        onAddDissectorFieldClicked();
    }
}

void MainWindow::refreshDissectorView() {
    QString baseStr = ui_dissectorBaseInput->text();
    if (baseStr.isEmpty()) return;
    
    uintptr_t base = 0;
    bool ok;
    if (baseStr.startsWith("0x")) base = baseStr.toULongLong(&ok, 16);
    else base = baseStr.toULongLong(&ok, 10);
    
    if (!ok || base == 0) return;

    for (int i = 0; i < ui_dissectorTable->rowCount(); ++i) {
        if (i >= (int)dissectorFields.size()) break;
        
        auto& field = dissectorFields[i];
        uintptr_t addr = base + field.offset;
        
        int size = 4;
        switch(field.type) {
            case DissectorType::Int8: case DissectorType::UInt8: size = 1; break;
            case DissectorType::Int16: case DissectorType::UInt16: size = 2; break;
            case DissectorType::Int32: case DissectorType::UInt32: case DissectorType::Float: size = 4; break;
            case DissectorType::Int64: case DissectorType::UInt64: case DissectorType::Double: case DissectorType::Pointer: size = 8; break;
            case DissectorType::String: size = 16; break;
        }
        
        std::vector<uint8_t> buf(size);
        if (engine.read_memory(addr, buf.data(), size)) {
            QString valStr, hexStr;
            for(int j=0; j<size; ++j) hexStr += QString("%1 ").arg(buf[j], 2, 16, QChar('0')).toUpper();
            
            switch(field.type) {
                case DissectorType::Int8: valStr = QString::number(*(int8_t*)buf.data()); break;
                case DissectorType::UInt8: valStr = QString::number(*(uint8_t*)buf.data()); break;
                case DissectorType::Int16: valStr = QString::number(*(int16_t*)buf.data()); break;
                case DissectorType::UInt16: valStr = QString::number(*(uint16_t*)buf.data()); break;
                case DissectorType::Int32: valStr = QString::number(*(int32_t*)buf.data()); break;
                case DissectorType::UInt32: valStr = QString::number(*(uint32_t*)buf.data()); break;
                case DissectorType::Int64: valStr = QString::number(*(int64_t*)buf.data()); break;
                case DissectorType::UInt64: valStr = QString::number(*(uint64_t*)buf.data()); break;
                case DissectorType::Float: valStr = QString::number(*(float*)buf.data()); break;
                case DissectorType::Double: valStr = QString::number(*(double*)buf.data()); break;
                case DissectorType::Pointer: valStr = QString("0x%1").arg(*(uintptr_t*)buf.data(), 0, 16).toUpper(); break;
                case DissectorType::String: {
                    std::string s((char*)buf.data(), size);
                    valStr = QString::fromStdString(s).simplified();
                    break;
                }
            }
            
            ui_dissectorTable->setItem(i, 2, new QTableWidgetItem(valStr));
            ui_dissectorTable->setItem(i, 3, new QTableWidgetItem(hexStr.trimmed()));
        } else {
            ui_dissectorTable->setItem(i, 2, new QTableWidgetItem("???"));
            ui_dissectorTable->setItem(i, 3, new QTableWidgetItem("?? ?? ?? ??"));
        }
    }
}

void MainWindow::onSearchGraphClicked() {
    QString text = ui_graphSearchInput->text().trimmed();
    if (text.isEmpty()) return;
    
    uintptr_t target = 0;
    bool ok;
    if (text.startsWith("0x")) target = text.toULongLong(&ok, 16);
    else target = text.toULongLong(&ok, 10);
    
    if (ok && graphNodes.count(target)) {
        auto* rect = graphNodes[target].rect;
        ui_graphView->centerOn(rect);
        rect->setSelected(true);
        
        // Visual ping effect
        auto* effect = new QGraphicsDropShadowEffect();
        effect->setColor(QColor("#bb9af7"));
        effect->setBlurRadius(20);
        rect->setGraphicsEffect(effect);
        QTimer::singleShot(2000, [rect]() { rect->setGraphicsEffect(nullptr); });
    } else {
        ui_statusBar->showMessage("Address not found in current graph.", 3000);
    }
}

void MainWindow::addNodeToGraph(uintptr_t addr, uintptr_t parentAddr) {
    if (graphNodes.count(addr)) return;
    
    // 1. Read and disassemble
    std::vector<uint8_t> buf(128);
    if (!engine.read_memory(addr, buf.data(), 128)) return;
    
    cs_insn *insn;
    size_t count = cs_disasm(capstoneHandle, buf.data(), 128, addr, 8, &insn);
    if (count == 0) return;
    
    GraphNode node;
    node.addr = addr;
    
    QString html = "<div style='font-family:monospace; line-height:1.1; font-size:9pt;'>";
    html += QString("<div style='background-color:#16161e; color:#7aa2f7; padding:4px; font-weight:bold;'>discovered_%1:</div>")
            .arg(addr, 8, 16, QChar('0')).toUpper();
    
    for(size_t i = 0; i < count; ++i) {
        QString addrStr = QString::number(insn[i].address, 16).toUpper().rightJustified(8, '0');
        QString mnemonic = QString(insn[i].mnemonic).leftJustified(6);
        QString ops = QString(insn[i].op_str);
        QString mColor = "#c0caf5";
        if (mnemonic.startsWith("j") || mnemonic == "call") mColor = "#f7768e";
        html += QString("<div style='padding:2px;'><span style='color:#565f89;'>%1</span> <span style='color:%2; font-weight:bold;'>%3</span> <span style='color:#9ece6a;'>%4</span></div>")
                .arg(addrStr, mColor, mnemonic, ops);
        
        if ((insn[i].mnemonic[0] == 'j' || std::string(insn[i].mnemonic) == "call") && ops.startsWith("0x")) {
            bool ok;
            uintptr_t target = ops.toULongLong(&ok, 16);
            if (ok) node.callees.push_back(target);
        }
    }
    html += "</div>";
    cs_free(insn, count);
    
    auto* text = new QGraphicsTextItem();
    text->setHtml(html);
    QRectF br = text->boundingRect();
    int nodeW = std::max(220, (int)br.width() + 10);
    int nodeH = br.height() + 10;
    
    // Position: find parent and place below/right
    int x = 0, y = 0;
    if (parentAddr > 0 && graphNodes.count(parentAddr)) {
        auto* pRect = graphNodes[parentAddr].rect;
        x = pRect->x() + 300;
        y = pRect->y() + 100;
        // Basic collision avoidance
        while(ui_graphScene->itemAt(x + 5, y + 5, QTransform())) { y += 150; }
    }
    
    auto* rect = ui_graphScene->addRect(x, y, nodeW, nodeH, QPen(QColor("#9ece6a"), 2), QBrush(QColor("#24283b")));
    rect->setFlag(QGraphicsItem::ItemIsMovable, true);
    rect->setFlag(QGraphicsItem::ItemIsSelectable, true);
    text->setParentItem(rect);
    text->setPos(x + 5, y + 5);
    node.rect = rect;
    
    graphNodes[addr] = node;
    
    // Draw Arrow from parent
    if (parentAddr > 0 && graphNodes.count(parentAddr)) {
        auto* pRect = graphNodes[parentAddr].rect;
        QPainterPath path;
        QPointF start = pRect->mapToScene(pRect->rect().center().x(), pRect->rect().bottom());
        QPointF end = rect->mapToScene(rect->rect().center().x(), rect->rect().top());
        path.moveTo(start);
        path.cubicTo(start.x(), start.y() + 40, end.x(), end.y() - 40, end.x(), end.y());
        
        auto* wire = ui_graphScene->addPath(path, QPen(QColor("#1a1b26"), 6));
        wire->setZValue(-2);
        auto* flow = ui_graphScene->addPath(path, QPen(QColor("#9ece6a"), 2, Qt::CustomDashLine));
        flow->setZValue(-1);
        
        GraphArrow gArr;
        gArr.wireItem = wire;
        gArr.flowItem = flow;
        auto* aHead = ui_graphScene->addPolygon(QPolygonF(), QPen(QColor("#9ece6a")), QBrush(QColor("#9ece6a")));
        gArr.arrowHead = aHead;
        gArr.sourceNode = pRect;
        gArr.targetNode = rect;
        gArr.infoText = nullptr;
        activeArrows.push_back(gArr);
    }
    
    // CRITICAL: If tracing is active, put a breakpoint on the newly discovered node!
    if (isTracing) {
        engine.set_breakpoint(addr);
    }
}
