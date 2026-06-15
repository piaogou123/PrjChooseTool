/*
 * @CopyRight: iNovatrol
 * @Description: Main window that lists Data_User.<ProjectId>.<Variant>
 *               folders. A horizontal project selector picks one project;
 *               its variants are shown below and the chosen one is written
 *               into the matching line of Data_System/ProjectDefinition.dat
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
class QScrollArea;
class QLabel;
class QButtonGroup;
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
    void OnApply();
    // A horizontal project option was picked -> show its variants.
    void OnProjectSelected();

    // Tray interactions.
    void OnTrayActivated(QSystemTrayIcon::ActivationReason reason);
    void ShowFromTray();
    void QuitApp();

private:
    void    BuildUi();
    void    SetupTray();

    // Scan baseDir for Data_User.* folders, grouped by project id.
    bool    ScanProjects(const QString& baseDir,
                         QMap<QString, ProjectInfo>* p_projects) const;

    // Read ProjectDefinition.dat into projectId -> active folder name.
    bool    LoadActiveVariants(const QString& datPath,
                               QMap<QString, QString>* p_active,
                               QStringList* p_duplicateIds) const;

    // Rebuild the horizontal row of project options.
    void    PopulateProjectBar();

    // Rebuild the variant list for one project (empty id clears it).
    void    PopulateVariantPanel(const QString& projectId);

    // Rewrite the matching line in ProjectDefinition.dat.
    bool    WriteDefinitions(const QString& datPath,
                             const QMap<QString, QString>& selections,
                             QString* p_message) const;

    QString DatFilePath() const;
    void    SetStatus(const QString& text);

    QLineEdit*   m_pBaseDirEdit{nullptr};
    QScrollArea* m_pProjectScroll{nullptr};   // horizontal project options
    QScrollArea* m_pVariantScroll{nullptr};   // variants of chosen project
    QLabel*      m_pVariantTitle{nullptr};
    QLabel*      m_pStatusLabel{nullptr};

    // Recreated 