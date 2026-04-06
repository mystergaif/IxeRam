#include "ProcessSelectorDialog.hpp"
#include <QHeaderView>
#include <QFile>
#include <QTextStream>
#include <dirent.h>
#include <algorithm>

ProcessSelectorDialog::ProcessSelectorDialog(QWidget *parent) : QDialog(parent) {
    setupUi();
    refreshProcessList();
}

void ProcessSelectorDialog::setupUi() {
    auto *layout = new QVBoxLayout(this);

    filterInput = new QLineEdit(this);
    filterInput->setPlaceholderText("Filter processes by name...");
    layout->addWidget(filterInput);

    processTable = new QTableWidget(0, 2, this);
    processTable->setHorizontalHeaderLabels({"PID", "Process Name"});
    processTable->horizontalHeader()->setStretchLastSection(true);
    processTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    processTable->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(processTable);

    auto *btnLayout = new QHBoxLayout();
    refreshButton = new QPushButton("Refresh", this);
    selectButton = new QPushButton("Select & Attach", this);
    selectButton->setEnabled(false);
    btnLayout->addWidget(refreshButton);
    btnLayout->addStretch();
    btnLayout->addWidget(selectButton);
    layout->addLayout(btnLayout);

    connect(filterInput, &QLineEdit::textChanged, this, &ProcessSelectorDialog::onFilterChanged);
    connect(refreshButton, &QPushButton::clicked, this, &ProcessSelectorDialog::refreshProcessList);
    connect(selectButton, &QPushButton::clicked, this, &ProcessSelectorDialog::onItemSelected);
    connect(processTable, &QTableWidget::itemDoubleClicked, this, &ProcessSelectorDialog::onItemSelected);
    connect(processTable, &QTableWidget::itemSelectionChanged, [this]() {
        selectButton->setEnabled(processTable->selectionModel()->hasSelection());
    });

    setWindowTitle("Attach to Process");
    resize(500, 600);
}

void ProcessSelectorDialog::refreshProcessList() {
    allProcesses.clear();
    DIR *dir = opendir("/proc");
    if (!dir) return;

    struct dirent *ent;
    while ((ent = readdir(dir)) != nullptr) {
        if (ent->d_type != DT_DIR) continue;
        
        QString pidStr(ent->d_name);
        bool isPid;
        pid_t pid = pidStr.toInt(&isPid);
        if (!isPid) continue;

        QString commPath = QString("/proc/%1/comm").arg(pid);
        QFile commFile(commPath);
        if (commFile.open(QIODevice::ReadOnly)) {
            QString name = commFile.readAll().trimmed();
            allProcesses.push_back({pid, name, ""});
        }
    }
    closedir(dir);

    // Sort by name
    std::sort(allProcesses.begin(), allProcesses.end(), [](const auto &a, const auto &b) {
        return a.name.toLower() < b.name.toLower();
    });

    onFilterChanged(filterInput->text());
}

void ProcessSelectorDialog::onFilterChanged(const QString &text) {
    processTable->setRowCount(0);
    for (const auto &proc : allProcesses) {
        if (text.isEmpty() || proc.name.contains(text, Qt::CaseInsensitive) || QString::number(proc.pid).contains(text)) {
            int row = processTable->rowCount();
            processTable->insertRow(row);
            processTable->setItem(row, 0, new QTableWidgetItem(QString::number(proc.pid)));
            processTable->setItem(row, 1, new QTableWidgetItem(proc.name));
        }
    }
}

void ProcessSelectorDialog::onItemSelected() {
    int row = processTable->currentRow();
    if (row >= 0) {
        selectedPid = processTable->item(row, 0)->text().toInt();
        accept();
    }
}
