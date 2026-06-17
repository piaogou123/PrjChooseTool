/*
 * @CopyRight: iNovatrol
 * @Description: Main window that lists Data_User.<ProjectId>.<Variant>
 *               folders grouped by project in a native list. Selecting a
 *               variant and confirming rewrites the matching line in
 *               Data_System/ProjectDefinition.dat
 * @version: <SET-YOUR-INITIALS> <SET-YOUR-VERSION>   // e.g. JCK J01
 * @Author: <SET-YOUR-NAME-IN-PINYIN>
 * @Date: 2026.06.15
 */

#ifndef PRJCHOOSETOOL_MAINWINDOW_H
#define PRJCHOOSETOOL_MAINWINDOW_H

#include <cstdint>

#include <QMainWindow>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QSystemTrayIcon>

// Forward declarations (HD-5: prefer forward-decl over include).
class QLineEdit;
class QTreeWidget;
class QTreeWidgetItem;
class QWidget;
class QCloseEvent;

namespace prjchoosetool
{

// One project (e.g. "F306") and every variant folder found on disk for it.
struct ProjectInfo
{
    QString     projectId;        // e.g. "F306"
    QStringList variantFolders;   // e.g. {"Data_User.F306.WFiber", ...}
};

class MainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* p_parent = nullptr);
    ~MainWindow() override;

protected:
    // Closing the window hides it to the tray instead of quitting.
    void closeEvent(QCloseEvent* p_event) override;

private slots:
    void OnBrowse();
    void OnReload();
    void OnSetActive();
    void OnTreeActivated(QTreeWidgetItem* p_item, int column);
    void OnCurrentItemChanged(QTreeWidgetItem* p_current,
                              QTreeWidgetItem* p_previous);
    void OnAbout();

    // Tray interactions.
    void OnTrayActivated(QSystemTrayIcon::ActivationReason reason);
    void ShowFromTray();
    void QuitApp();

private:
    void    BuildUi();
    void    BuildMenu();
    void    SetupTray();

    // Scan baseDir for Data_User.* folders, grouped by project id.
    bool    ScanProjects(const QString& baseDir,
                         QMap<QString, ProjectInfo>* p_projects) const;

    // Read ProjectDefinition.dat into projectId -> active folder name.
    bool    LoadActiveVariants(const QString& datPath,
                               QMap<QString, QString>* p_active,
                               QStringList* p_duplicateIds) const;

    // Rebuild the grouped project / variant tree.
    void    PopulateTree();

    // Re-select a variant row by its folder name after a refresh.
    void    SelectVariant(const QString& folder);

    // Rewrite the matching line in ProjectDefinition.dat.
    bool    WriteDefinitions(const QString& datPath,
                             const QMap<QString, QString>& selections,
                             QString* p_message) const;

    QString DatFilePath() const;
    void    SetStatus(const QString& text);

    QLineEdit*   m_pBaseDirEdit{nullptr};
    QTreeWidget* m_pTree{nullptr};

    // System-tray support for "close hides to background".
    QSystemTrayIcon* m_pTrayIcon{nullptr};
    bool             m_forceQuit{false};
    bool             m_trayHintShown{false};

    // Cached scan results.
    QMap<QString, ProjectInfo> m_projects;
    QMap<QString, QString>     m_activeVariants;
};

}  // namespace prjchoosetool

#endif  // PRJCHOOSETOOL_MAINWINDOW_H
