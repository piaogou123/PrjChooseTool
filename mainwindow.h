/*
 * @CopyRight: iNovatrol
 * @Description: Main window that lists Data_User.<ProjectId>.<Variant>
 *               folders grouped by project in a native list. Selecting a
 *               variant rewrites the matching line in
 *               Data_System/ProjectDefinition.dat. Supports English / Chinese.
 * @version: <SET-YOUR-INITIALS> <SET-YOUR-VERSION>   // e.g. JCK J01
 * @Author: <SET-YOUR-NAME-IN-PINYIN>
 * @Date: 2026.06.15
 */

#ifndef PRJCHOOSETOOL_MAINWINDOW_H
#define PRJCHOOSETOOL_MAINWINDOW_H

#include <cstdint>

#include <QHash>
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
class QLabel;
class QPushButton;
class QMenu;
class QAction;
class QVBoxLayout;

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
    void OnLanguageSelected(QAction* p_action);
    void OnLicenseSettings();
    void OnLicenseKeyEdited();

    // Tray interactions.
    void OnTrayActivated(QSystemTrayIcon::ActivationReason reason);
    void ShowFromTray();
    void QuitApp();

private:
    void    BuildUi();
    void    BuildMenu();
    void    SetupTray();

    // Translation helpers (lightweight, no .ts/.qm needed).
    void    BuildTranslations();
    QString Tr(const QString& english) const;
    void    RetranslateUi();

    // Per-project license verification.
    void    BuildLicenseRows();
    QString LicenseFilePath(const QString& projectId) const;
    void    RunLicenseCheck(const QString& projectId);
    void    RunAllLicenseChecks();

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

    // True if "i-Novatrol <projectId>.exe" is currently running.
    bool    IsProjectRunning(const QString& projectId) const;

    // Rewrite the matching line in ProjectDefinition.dat.
    bool    WriteDefinitions(const QString& datPath,
                             const QMap<QString, QString>& selections,
                             QString* p_message) const;

    QString DatFilePath() const;
    void    SetStatus(const QString& text);

    QLineEdit*   m_pBaseDirEdit{nullptr};
    QTreeWidget* m_pTree{nullptr};

    // Per-project license inputs (one row per enabled project).
    QWidget*     m_pLicenseContainer{nullptr};
    QVBoxLayout* m_pLicenseLayout{nullptr};
    QHash<QString, QLineEdit*> m_licenseEdits;
    QStringList  m_licenseProjects;   // project ids with license check enabled

    // Retranslatable widgets / actions.
    QLabel*      m_pDirLabel{nullptr};
    QPushButton* m_pBrowseButton{nullptr};
    QPushButton* m_pReloadButton{nullptr};
    QPushButton* m_pSetButton{nullptr};

    QMenu*   m_pFileMenu{nullptr};
    QMenu*   m_pActionsMenu{nullptr};
    QMenu*   m_pHelpMenu{nullptr};
    QMenu*   m_pLangMenu{nullptr};
    QAction* m_pReloadAction{nullptr};
    QAction* m_pExitAction{nullptr};
    QAction* m_pSetAction{nullptr};
    QAction* m_pAboutAction{nullptr};
    QAction* m_pLangEnAction{nullptr};
    QAction* m_pLangZhAction{nullptr};
    QAction* m_pLicenseSettingsAction{nullptr};
    QAction* m_pTrayShowAction{nullptr};
    QAction* m_pTrayQuitAction{nullptr};

    // System-tray support for "close hides to background".
    QSystemTrayIcon* m_pTrayIcon{nullptr};
    bool             m_forceQuit{false};
    bool             m_trayHintShown{false};

    // Language: 0 = English, 1 = Chinese.
    int32_t                    m_lang{0};
    QHash<QString, QString>    m_zh;

    // Cached scan results.
    QMap<QString, ProjectInfo> m_projects;
    QMap<QString, QString>     m_activeVariants;
};

}  // namespace prjchoosetool

#endif  // PRJCHOOSETOOL_MAINWINDOW_H
