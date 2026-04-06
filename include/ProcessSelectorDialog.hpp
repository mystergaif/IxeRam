#pragma once

#include <QDialog>
#include <QTableWidget>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QPushButton>
#include <vector>

struct ProcessInfo {
    pid_t pid;
    QString name;
    QString cmdline;
};

class ProcessSelectorDialog : public QDialog {
    Q_OBJECT

public:
    ProcessSelectorDialog(QWidget *parent = nullptr);
    pid_t getSelectedPid() const { return selectedPid; }

private slots:
    void onFilterChanged(const QString &text);
    void onItemSelected();
    void refreshProcessList();

private:
    void setupUi();
    std::vector<ProcessInfo> getAllProcesses();

    QLineEdit *filterInput;
    QTableWidget *processTable;
    QPushButton *selectButton;
    QPushButton *refreshButton;

    std::vector<ProcessInfo> allProcesses;
    pid_t selectedPid = -1;
};
