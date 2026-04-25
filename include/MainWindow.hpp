#pragma once

#include <QMainWindow>
#include <QSlider>

#include <QPushButton>
#include <QVBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QTableWidget>
#include <QStatusBar>
#include <QComboBox>
#include <QTimer>
#include <unistd.h>
#include <QTabWidget>
#include <QSplitter>
#include <QTextEdit>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsRectItem>
#include <QGraphicsPathItem>
#include <QWheelEvent>
#include <QtMath>

#include <capstone/capstone.h>
#include "MemoryEngine.hpp"
#include "Scanner.hpp"
#include "Config.hpp"
#include <atomic>
#include <thread>
#include <QGraphicsDropShadowEffect>
struct CategorizedAddress {
    uintptr_t addr;
    std::string module_name;
    uintptr_t base_addr;
    uintptr_t offset;
    std::string value;
};

// Simple Graph Node structure
struct GraphNode {
    uintptr_t addr;
    QString label;
    QGraphicsRectItem* rect;
    std::vector<uintptr_t> callees;
};

enum class DissectorType {
    Int8, UInt8, Int16, UInt16, Int32, UInt32, Int64, UInt64, Float, Double, Pointer, String
};

struct DissectorField {
    int offset;
    DissectorType type;
    QString description;
};

class ZoomableView : public QGraphicsView {
public:
    ZoomableView(QGraphicsScene* scene, QWidget* parent = nullptr) 
        : QGraphicsView(scene, parent) {
        setRenderHint(QPainter::Antialiasing);
        setDragMode(QGraphicsView::ScrollHandDrag);
        setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    }
protected:
    void wheelEvent(QWheelEvent* event) override {
        if (event->modifiers() & Qt::ControlModifier) {
            double angle = event->angleDelta().y();
            double factor = qPow(1.0015, angle);
            scale(factor, factor);
        } else {
            QGraphicsView::wheelEvent(event);
        }
    }
};

class SpeedClockWidget : public QWidget {
    Q_OBJECT
public:
    SpeedClockWidget(QWidget* parent = nullptr) : QWidget(parent) {
        setFixedSize(200, 200);
        timer = new QTimer(this);
        connect(timer, &QTimer::timeout, this, [this]() {
            angle += speed * 5.0;
            update();
        });
        timer->start(16); // ~60 FPS
    }
    void setSpeed(double s) { speed = s; }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.translate(width() / 2, height() / 2);

        // Outer ring
        painter.setPen(QPen(QColor("#414868"), 8));
        painter.drawEllipse(-80, -80, 160, 160);

        // Glow
        QRadialGradient gradient(0, 0, 80);
        gradient.setColorAt(0, QColor(122, 162, 247, 20));
        gradient.setColorAt(1, QColor(122, 162, 247, 100));
        painter.setBrush(gradient);
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(-80, -80, 160, 160);

        // Clock hand
        painter.rotate(angle);
        painter.setPen(QPen(QColor("#7aa2f7"), 4, Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(0, 0, 0, -70);
        
        // Center dot
        painter.setBrush(QColor("#bb9af7"));
        painter.drawEllipse(-5, -5, 10, 10);
    }

private:
    QTimer* timer;
    double angle = 0;
    double speed = 1.0;
};

class MainWindow : public QMainWindow {

    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onScanClicked();
    void onResetClicked();
    void onProcessSelectClicked();
    void updateUiData();
    void onResultDoubleClicked(int row, int column);
    void onTabChanged(int index);
    void onBuildGraphClicked();
    void onSearchGraphClicked();
    
    // New Feature Slots
    void onAddWatchClicked();
    void onRunPtrScanClicked();
    void onSetBreakpointClicked();
    void onGotoClicked();
    void onAddDissectorFieldClicked();
    void onClearDissectorClicked();
    void onFillDissectorClicked();
    
    // Speedhack slots
    void onSpeedSliderChanged(int value);
    void onSpeedPauseClicked();
    void onSpeedResetClicked();
    void onSpeedInjectClicked();



private:
    void setupUi();
    void refreshScannerTable();
    void updateScannerLiveValues();
    void refreshMemoryMap();
    void refreshDisasmView();
    void refreshHexView();
    void refreshDissectorView();
    void buildCallGraph(uintptr_t root);
    void addNodeToGraph(uintptr_t addr, uintptr_t parentAddr = 0);
    void startLiveTrace();
    
    void refreshWatchlist();
    void refreshPtrScanTable();
    void refreshBpHitsTable();
    
    // Core Engine
    MemoryEngine engine;
    Scanner scanner;
    IxeRamConfig config;
    csh capstoneHandle;

    // UI Structure
    QWidget *ui_centralWidget;
    QVBoxLayout *ui_mainLayout;
    QTabWidget *ui_tabWidget;
    
    // TAB 1: Scanner
    QWidget *ui_scannerTab;
    QVBoxLayout *ui_scannerLayout;
    QComboBox *ui_valueTypeCombo;
    QComboBox *ui_scanTypeCombo;
    QLineEdit *ui_searchValueInput;
    QPushButton *ui_scanButton;
    QPushButton *ui_resetButton;
    QTableWidget *ui_resultsTable;
    
    QLabel *ui_processInfoLabel;
    QPushButton *ui_selectProcessButton;
    
    // TAB 2: Memory Map
    QWidget *ui_mapTab;
    QVBoxLayout *ui_mapLayout;
    QTextEdit *ui_mapText;
    
    // TAB 3: Debug (Disasm/Hex)
    QWidget *ui_debugTab;
    QVBoxLayout *ui_debugLayout;
    QHBoxLayout *ui_gotoLayout;
    QLineEdit *ui_gotoInput;
    QPushButton *ui_gotoButton;
    QSplitter *ui_debugSplitter;
    QTextEdit *ui_disasmText;
    QTableWidget *ui_hexTable;

    // TAB 4: Graph
    QWidget *ui_graphTab;
    QVBoxLayout *ui_graphLayout;
    QHBoxLayout *ui_graphControlsLayout;
    ZoomableView *ui_graphView;
    QGraphicsScene *ui_graphScene;
    QPushButton *ui_buildGraphButton;
    QPushButton *ui_traceGraphButton; // Live trace button
    QLineEdit *ui_graphSearchInput;
    QPushButton *ui_graphSearchButton;
    
    // TAB 5: Watchlist
    QWidget *ui_watchTab;
    QVBoxLayout *ui_watchLayout;
    QTableWidget *ui_watchTable;
    QPushButton *ui_addWatchButton;
    
    // TAB 6: Pointer Scan
    QWidget *ui_ptrTab;
    QVBoxLayout *ui_ptrLayout;
    QTableWidget *ui_ptrTable;
    QPushButton *ui_runPtrScanButton;
    
    // TAB 7: HW Breakpoints
    QWidget *ui_bpTab;
    QVBoxLayout *ui_bpLayout;
    QTableWidget *ui_bpTable;
    QPushButton *ui_setBpButton;

    // TAB 8: Structure Dissector
    QWidget *ui_dissectorTab;
    QVBoxLayout *ui_dissectorLayout;
    QLineEdit *ui_dissectorBaseInput;
    QPushButton *ui_dissectorAddButton;
    QPushButton *ui_dissectorClearButton;
    QPushButton *ui_dissectorFillButton;
    QTableWidget *ui_dissectorTable;
    
    // TAB 9: Speedhack
    QWidget *ui_speedhackTab;
    QVBoxLayout *ui_speedhackLayout;
    SpeedClockWidget *ui_speedClock;
    QLabel *ui_speedValueLabel;
    QSlider *ui_speedSlider;
    QPushButton *ui_speedPauseButton;
    QPushButton *ui_speedResetButton;
    QPushButton *ui_speedInjectButton;

    
    // Shared Status
    QStatusBar *ui_statusBar;
    QTimer *ui_uiRefreshTimer;
    
    // Data Caching
    uintptr_t currentDebugAddr = 0;
    std::vector<DissectorField> dissectorFields;
    std::map<uintptr_t, GraphNode> graphNodes;

    struct GraphArrow {
        QGraphicsPathItem* wireItem;
        QGraphicsPathItem* flowItem;
        QGraphicsPolygonItem* arrowHead;
        QGraphicsTextItem* infoText;
        QGraphicsRectItem* sourceNode;
        QGraphicsRectItem* targetNode;
        
        int hitCount = 0;
        AccessRecord lastData;
        bool hasData = false;
    };
    std::vector<GraphArrow> activeArrows;
    
    // Live Tracing
    std::atomic<bool> isTracing{false};
    std::thread tracerThread;
};
