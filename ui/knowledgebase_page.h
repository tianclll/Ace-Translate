#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QLabel>
#include <QPushButton>

class DropZoneWidget;

class KnowledgeBasePage : public QWidget {
    Q_OBJECT
public:
    explicit KnowledgeBasePage(QWidget* parent = nullptr);
    ~KnowledgeBasePage() override;

    void refreshList();

signals:
    void statusMessage(const QString& msg);

private slots:
    void onFileDropped(const QStringList& paths);
    void onViewEntry(int id);
    void onDeleteEntry(int id);
    void onExportEntry(int id);

private:
    void setupUI();
    QWidget* createListItem(int id, const QString& title, const QString& type,
                            const QString& date, const QString& lang);

    DropZoneWidget* dropZone_;
    QWidget* listContainer_;
    QVBoxLayout* listLayout_;
    QLabel* countLabel_;
    QScrollArea* scrollArea_;
    QLabel* emptyHint_;
};
