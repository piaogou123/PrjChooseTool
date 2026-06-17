/*
 * @CopyRight: iNovatrol
 * @Description: Implementation of the project-variant chooser window
 * @version: <SET-YOUR-INITIALS> <SET-YOUR-VERSION>   // e.g. JCK J01
 * @Author: <SET-YOUR-NAME-IN-PINYIN>
 * @Date: 2026.06.15
 */

#include "mainwindow.h"   // HD-10: own header first

#include <cstdint>

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QStatusBar>
#include <QSystemTrayIcon>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QTreeWidgetItemIterator>
#include <QVBoxLayout>
#include <QWidget>

namespace prjchoosetool
{

namespace
{

// Folder prefix that marks a user-data variant folder.
const QString FOLDER_PREFIX = QStringLiteral("Data_User.");

// Sub-path of the definition file relative to the working directory.
const QString DEFINITION_SUBDIR  = QStringLiteral("Data_System");
const QString DEFINITION_FILE    = QStringLiteral("ProjectDefinition.dat");

// QSettings key under which the last-used working folder is stored.
const QString SETTINGS_LAST_DIR =
    QStringLiteral("workingDir/lastPath");

// Item data roles for tree rows.
const int32_t ROLE_FOLDER  = Qt::UserRole;       // full Data_User.* folder
const int32_t ROLE_PROJECT = Qt::UserRole + 1;   // owning project id

// GeekUninstaller-like flat, native, light theme.
const char* const APP_STYLE =
    "QMainWindow, QWidget { background: #FFFFFF; color: #000000;"
    "  font-family: 'Segoe UI', 'Microsoft YaHei', sans-serif;"
    "  font-size: 9pt; }"
    "QMenuBar { background: #F5F5F5; border-bottom: 1px solid #DCDCDC; }"
    "QMenuBar::item { padding: 4px 10px; background: transparent; }"
    "QMenuBar::item:selected { background: #CBE8F6; }"
    "QMenu { background: #FFFFFF; border: 1px solid #ABADB3; }"
    "QMenu::item { padding: 4px 24px; }"
    "QMenu::item:selected { background: #CBE8F6; }"
    "QLineEdit { border: 1px solid #ABADB3; padding: 2px 4px;"
    "  background: #FFFFFF; }"
    "QLineEdit:focus { border: 1px solid #3399FF; }"
    "QPushButton { border: 1px solid #ABADB3; padding: 3px 12px;"
    "  background: #F5F5F5; min-height: 18px; }"
    "QPushButton:hover { background: #E5F1FB; border-color: #3399FF; }"
    "QPushButton:pressed { background: #CCE4F7; }"
    "QPushButton:default { border-color: #3399FF; }"
    "QTreeWidget { border: 1px solid #ABADB3; outline: 0;"
    "  alternate-background-color: #F7F7F7; }"
    "QTreeView::item { height: 20px; border: 0px; }"
    "QTreeView::item:selected { background: #CBE8F6; color: #000000; }"
    "QTreeView::item:selected:!active { background: #E8E8E8; }"
    "QHeaderView::section { background: #F5F5F5; padding: 3px 6px;"
    "  border: 0px; border-right: 1px solid #E2E2E2;"
    "  border-bottom: 1px solid #DCDCDC; }"
    "QStatusBar { background: #F5F5F5; border-top: 1px solid #DCDCDC; }"
    "QStatusBar::item { border: 0px; }";

// Extract the project id from a folder name such as
// "Data_User.F306.WFiber2" -> "F306". Returns an empty string if the
// folder does not match the expected pattern.
QString ParseProjectId(const QString& folderName)
{
    if (!folderName.startsWith(FOLDER_PREFIX)) {
        return QString();
    }

    const QString remainder = folderName.mid(FOLDER_PREFIX.length());
    const int32_t dotPos = static_cast<int32_t>(remainder.indexOf('.'));
    if (dotPos <= 0) {
        // Need <ProjectId>.<Variant>.
        return QString();
    }

    const QString projectId = remainder.left(dotPos).trimmed();
    const QString variant   = remainder.mid(dotPos + 1).trimmed();
    if (projectId.isEmpty() || variant.isEmpty()) {
        return QString();
    }
    return projectId;
}

}  // namespace

MainWindow::MainWindow(QWidget* p_parent)
    : QMainWindow(p_parent)
{
    BuildUi();
    BuildMenu();
    SetupTray();
    OnReload();
}

MainWindow::~MainWindow()
{
    // Child widgets and button groups are owned by Qt's parent hierarchy,
    // so no manual cleanup is required here.
}

void MainWindow::BuildUi()
{
    setWindowTitle(QStringLiteral("Project Chooser"));
    setWindowIcon(QIcon(QStringLiteral(":/appicon.png")));
    setStyleSheet(QString::fromUtf8(APP_STYLE));
    resize(580, 460);

    QWidget* p_central = new QWidget(this);
    setCentralWidget(p_central);

    QVBoxLayout* p_mainLayout = new QVBoxLayout(p_central);
    p_mainLayout->setContentsMargins(8, 8, 8, 8);
    p_mainLayout->setSpacing(6);

    // --- Top row: working directory + Browse ------------------------
    QHBoxLayout* p_topRow = new QHBoxLayout();
    p_topRow->setSpacing(6);

    QLabel* p_dirLabel = new QLabel(QStringLiteral("Folder:"), p_central);

    QSettings settings;
    const QString savedDir = settings.value(SETTINGS_LAST_DIR).toString();
    m_pBaseDirEdit = new QLineEdit(savedDir, p_central);
    m_pBaseDirEdit->setPlaceholderText(
        QStringLiteral("Select your working folder..."));

    QPushButton* p_browseButton =
        new QPushButton(QStringLiteral("Browse..."), p_central);
    QPushButton* p_reloadButton =
        new QPushButton(QStringLiteral("Reload"), p_central);

    p_topRow->addWidget(p_dirLabel);
    p_topRow->addWidget(m_pBaseDirEdit, 1);
    p_topRow->addWidget(p_browseButton);
    p_topRow->addWidget(p_reloadButton);
    p_mainLayout->addLayout(p_topRow);

    // --- Project / variant tree ------------------------------------
    m_pTree = new QTreeWidget(p_central);
    m_pTree->setColumnCount(2);
    QStringList headers;
    headers << QStringLiteral("Project / Variant") << QStringLiteral("Status");
    m_pTree->setHeaderLabels(headers);
    m_pTree->setRootIsDecorated(true);
    m_pTree->setUniformRowHeights(true);
    m_pTree->setAlternatingRowColors(true);
    m_pTree->setAllColumnsShowFocus(true);
    m_pTree->setExpandsOnDoubleClick(false);
    m_pTree->header()->setStretchLastSection(false);
    m_pTree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_pTree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    p_mainLayout->addWidget(m_pTree, 1);

    // --- Bottom row: Set-active button -----------------------------
    QHBoxLayout* p_bottomRow = new QHBoxLayout();
    p_bottomRow->addStretch(1);

    QPushButton* p_setButton =
        new QPushButton(QStringLiteral("Set as active"), p_central);
    p_setButton->setDefault(true);
    p_bottomRow->addWidget(p_setButton);
    p_mainLayout->addLayout(p_bottomRow);

    statusBar()->showMessage(QStringLiteral("Ready"));

    connect(p_browseButton, &QPushButton::clicked,
            this, &MainWindow::OnBrowse);
    connect(p_reloadButton, &QPushButton::clicked,
            this, &MainWindow::OnReload);
    connect(p_setButton, &QPushButton::clicked,
            this, &MainWindow::OnSetActive);
    connect(m_pTree, &QTreeWidget::itemActivated,
            this, &MainWindow::OnTreeActivated);
    connect(m_pTree, &QTreeWidget::itemDoubleClicked,
            this, &MainWindow::OnTreeActivated);
    connect(m_pTree, &QTreeWidget::currentItemChanged,
            this, &MainWindow::OnCurrentItemChanged);
}

void MainWindow::BuildMenu()
{
    QMenu* p_fileMenu = menuBar()->addMenu(QStringLiteral("&File"));
    QAction* p_reloadAction =
        p_fileMenu->addAction(QStringLiteral("&Reload"));
    p_reloadAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+R")));
    p_fileMenu->addSeparator();
    QAction* p_exitAction = p_fileMenu->addAction(QStringLiteral("E&xit"));

    QMenu* p_actionsMenu = menuBar()->addMenu(QStringLiteral("&Actions"));
    QAction* p_setAction =
        p_actionsMenu->addAction(QStringLiteral("&Set as active"));
    p_setAction->setShortcut(QKeySequence(QStringLiteral("Return")));

    QMenu* p_helpMenu = menuBar()->addMenu(QStringLiteral("&Help"));
    QAction* p_aboutAction =
        p_helpMenu->addAction(QStringLiteral("&About"));

    connect(p_reloadAction, &QAction::triggered, this, &MainWindow::OnReload);
    connect(p_exitAction, &QAction::triggered, this, &MainWindow::QuitApp);
    connect(p_setAction, &QAction::triggered, this, &MainWindow::OnSetActive);
    connect(p_aboutAction, &QAction::triggered, this, &MainWindow::OnAbout);
}

void MainWindow::SetupTray()
{
    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        // No tray: closing will quit normally (see closeEvent).
        return;
    }

    // Keep the app alive after the window is hidden to the tray.
    QApplication::setQuitOnLastWindowClosed(false);

    m_pTrayIcon = new QSystemTrayIcon(
        QIcon(QStringLiteral(":/appicon.png")), this);
    m_pTrayIcon->setToolTip(QStringLiteral("Project Chooser"));

    QMenu* p_menu = new QMenu(this);
    QAction* p_showAction =
        p_menu->addAction(QStringLiteral("Show window"));
    p_menu->addSeparator();
    QAction* p_quitAction =
        p_menu->addAction(QStringLiteral("Quit"));

    m_pTrayIcon->setContextMenu(p_menu);
    m_pTrayIcon->show();

    connect(p_showAction, &QAction::triggered,
            this, &MainWindow::ShowFromTray);
    connect(p_quitAction, &QAction::triggered,
            this, &MainWindow::QuitApp);
    connect(m_pTrayIcon, &QSystemTrayIcon::activated,
            this, &MainWindow::OnTrayActivated);
}

void MainWindow::closeEvent(QCloseEvent* p_event)
{
    if (p_event == nullptr) {
        return;
    }

    // A real quit (from the tray menu) or no tray available -> close.
    if (m_forceQuit || m_pTrayIcon == nullptr) {
        p_event->accept();
        return;
    }

    // Otherwise hide to the tray and keep running in the background.
    hide();
    p_event->ignore();

    if (!m_trayHintShown) {
        m_pTrayIcon->showMessage(
            QStringLiteral("Project Chooser"),
            QStringLiteral("Still running in the tray. "
                           "Right-click the icon to quit."),
            QSystemTrayIcon::Information, 3000);
        m_trayHintShown = true;
    }
}

void MainWindow::OnTrayActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::Trigger
        || reason == QSystemTrayIcon::DoubleClick) {
        ShowFromTray();
    }
}

void MainWindow::ShowFromTray()
{
    showNormal();
    raise();
    activateWindow();
}

void MainWindow::QuitApp()
{
    m_forceQuit = true;
    QApplication::quit();
}

void MainWindow::OnAbout()
{
    QMessageBox::about(
        this, QStringLiteral("About Project Chooser"),
        QStringLiteral("<b>Project Chooser</b><br>"
                       "Switches the active Data_User variant for a project "
                       "by editing Data_System/ProjectDefinition.dat.<br><br>"
                       "iNovatrol"));
}

QString MainWindow::DatFilePath() const
{
    const QString baseDir = m_pBaseDirEdit->text().trimmed();
    return QDir(baseDir).filePath(
        DEFINITION_SUBDIR + "/" + DEFINITION_FILE);
}

void MainWindow::SetStatus(const QString& text)
{
    statusBar()->showMessage(text);
}

void MainWindow::OnBrowse()
{
    const QString chosen = QFileDialog::getExistingDirectory(
        this, QStringLiteral("Select working folder"),
        m_pBaseDirEdit->text().trimmed());
    if (!chosen.isEmpty()) {
        m_pBaseDirEdit->setText(chosen);
        OnReload();
    }
}

void MainWindow::OnReload()
{
    m_projects.clear();
    m_activeVariants.clear();

    const QString baseDir = m_pBaseDirEdit->text().trimmed();
    if (baseDir.isEmpty()) {
        SetStatus(QStringLiteral("Select a working folder (Browse...)."));
        PopulateTree();
        return;
    }
    if (!QDir(baseDir).exists()) {
        SetStatus(QStringLiteral("Working folder does not exist: ") + baseDir);
        PopulateTree();
        return;
    }

    // Remember this folder for next launch.
    QSettings settings;
    settings.setValue(SETTINGS_LAST_DIR, baseDir);

    ScanProjects(baseDir, &m_projects);

    QStringList duplicateIds;
    const bool datOk =
        LoadActiveVariants(DatFilePath(), &m_activeVariants, &duplicateIds);

    PopulateTree();

    int32_t variantCount = 0;
    for (auto it = m_projects.constBegin();
         it != m_projects.constEnd(); ++it) {
        variantCount += static_cast<int32_t>(it.value().variantFolders.size());
    }

    QString status = QStringLiteral("%1 project(s), %2 variant(s).")
                         .arg(m_projects.size())
                         .arg(variantCount);
    if (!datOk) {
        status += QStringLiteral("  ProjectDefinition.dat not found.");
    }
    if (!duplicateIds.isEmpty()) {
        status += QStringLiteral("  Duplicate ids: ")
                  + duplicateIds.join(QStringLiteral(", "));
    }
    SetStatus(status);
}

bool MainWindow::ScanProjects(const QString& baseDir,
                              QMap<QString, ProjectInfo>* p_projects) const
{
    if (p_projects == nullptr) {
        return false;
    }
    p_projects->clear();

    QDir dir(baseDir);
    if (!dir.exists()) {
        return false;
    }

    const QStringList entries =
        dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);

    for (const QString& entry : entries) {
        const QString projectId = ParseProjectId(entry);
        if (projectId.isEmpty()) {
            continue;
        }

        ProjectInfo& info = (*p_projects)[projectId];
        info.projectId = projectId;
        info.variantFolders.append(entry);
    }
    return true;
}

bool MainWindow::LoadActiveVariants(const QString& datPath,
                                    QMap<QString, QString>* p_active,
                                    QStringList* p_duplicateIds) const
{
    if (p_active == nullptr || p_duplicateIds == nullptr) {
        return false;
    }
    p_active->clear();
    p_duplicateIds->clear();

    QFile file(datPath);
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
        return false;
    }

    const QString content = QString::fromUtf8(file.readAll());
    file.close();

    const QStringList lines = content.split('\n');
    for (const QString& rawLine : lines) {
        const QString line = rawLine.trimmed();
        if (line.isEmpty()) {
            continue;
        }

        const int32_t commaPos = static_cast<int32_t>(line.indexOf(','));
        if (commaPos <= 0) {
            continue;
        }

        const QString id     = line.left(commaPos).trimmed();
        const QString folder = line.mid(commaPos + 1).trimmed();
        if (id.isEmpty()) {
            continue;
        }

        if (p_active->contains(id)) {
            if (!p_duplicateIds->contains(id)) {
                p_duplicateIds->append(id);
            }
            continue;   // Keep the first occurrence as the active one.
        }
        p_active->insert(id, folder);
    }
    return true;
}

void MainWindow::PopulateTree()
{
    m_pTree->clear();

    for (auto it = m_projects.constBegin();
         it != m_projects.constEnd(); ++it) {
        const ProjectInfo& info = it.value();

        QTreeWidgetItem* p_top = new QTreeWidgetItem(m_pTree);
        p_top->setText(0, info.projectId);
        // Group rows are headers, not selectable targets.
        p_top->setFlags(Qt::ItemIsEnabled);
        QFont topFont = p_top->font(0);
        topFont.setBold(true);
        p_top->setFont(0, topFont);

        const QString activeFolder = m_activeVariants.value(info.projectId);
        bool activeMatched = false;

        for (const QString& folder : info.variantFolders) {
            QTreeWidgetItem* p_child = new QTreeWidgetItem(p_top);
            p_child->setText(0, folder);
            p_child->setData(0, ROLE_FOLDER, folder);
            p_child->setData(0, ROLE_PROJECT, info.projectId);

            if (folder == activeFolder) {
                p_child->setText(1, QStringLiteral("Active"));
                QFont f = p_child->font(0);
                f.setBold(true);
                p_child->setFont(0, f);
                p_child->setFont(1, f);
                activeMatched = true;
            }
        }

        if (!activeFolder.isEmpty() && !activeMatched) {
            p_top->setText(1, QStringLiteral("missing"));
        }

        p_top->setExpanded(true);
    }
}

void MainWindow::SelectVariant(const QString& folder)
{
    if (folder.isEmpty()) {
        return;
    }

    QTreeWidgetItemIterator it(m_pTree);
    while (*it != nullptr) {
        QTreeWidgetItem* p_item = *it;
        if (p_item->data(0, ROLE_FOLDER).toString() == folder) {
            m_pTree->setCurrentItem(p_item);
            m_pTree->scrollToItem(p_item);
            break;
        }
        ++it;
    }
}

void MainWindow::OnTreeActivated(QTreeWidgetItem* p_item, int column)
{
    Q_UNUSED(column);
    if (p_item == nullptr) {
        return;
    }
    // Only variant rows carry a folder; ignore group headers.
    if (p_item->data(0, ROLE_FOLDER).toString().isEmpty()) {
        return;
    }
    OnSetActive();
}

void MainWindow::OnCurrentItemChanged(QTreeWidgetItem* p_current,
                                      QTreeWidgetItem* p_previous)
{
    Q_UNUSED(p_previous);
    if (p_current == nullptr) {
        return;
    }

    const QString folder    = p_current->data(0, ROLE_FOLDER).toString();
    const QString projectId = p_current->data(0, ROLE_PROJECT).toString();
    if (folder.isEmpty() || projectId.isEmpty()) {
        // A project header is selected, not a variant.
        SetStatus(QStringLiteral("Select a variant under a project."));
        return;
    }

    SetStatus(QStringLiteral("Target: project %1 -> %2  "
                             "(click \"Set as active\")")
                  .arg(projectId, folder));
}

void MainWindow::OnSetActive()
{
    QTreeWidgetItem* p_item = m_pTree->currentItem();
    if (p_item == nullptr) {
        SetStatus(QStringLiteral("Select a variant first."));
        return;
    }

    const QString folder    = p_item->data(0, ROLE_FOLDER).toString();
    const QString projectId = p_item->data(0, ROLE_PROJECT).toString();
    if (folder.isEmpty() || projectId.isEmpty()) {
        SetStatus(QStringLiteral("Select a variant row, not a project."));
        return;
    }

    QMap<QString, QString> selections;
    selections.insert(projectId, folder);

    QString message;
    const bool ok = WriteDefinitions(DatFilePath(), selections, &message);
    if (!ok) {
        QMessageBox::warning(this, QStringLiteral("Update failed"), message);
        SetStatus(message);
        return;
    }

    // Refresh, then keep the user on the row they just activated.
    OnReload();
    SelectVariant(folder);
    SetStatus(QStringLiteral("Set %1 -> %2   (%3)")
                  .arg(projectId, folder, message));
}

bool MainWindow::WriteDefinitions(const QString& datPath,
                                  const QMap<QString, QString>& selections,
                                  QString* p_message) const
{
    if (p_message == nullptr) {
        return false;
    }

    QFile file(datPath);
    if (!file.exists()) {
        *p_message = QStringLiteral("File not found: ") + datPath;
        return false;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        *p_message = QStringLiteral("Cannot open for reading: ") + datPath;
        return false;
    }
    const QString content = QString::fromUtf8(file.readAll());
    file.close();

    // Preserve the original line ending style.
    const QString eol =
        content.contains(QStringLiteral("\r\n"))
            ? QStringLiteral("\r\n")
            : QStringLiteral("\n");

    // Make a backup before touching the original file.
    const QString backupPath = datPath + QStringLiteral(".bak");
    QFile::remove(backupPath);
    QFile::copy(datPath, backupPath);

    const QStringList lines = content.split(eol);
    QStringList outLines;
    outLines.reserve(lines.size());

    QStringList updatedIds;
    QStringList duplicateIds;
    QMap<QString, bool> doneIds;

    for (const QString& line : lines) {
        const int32_t commaPos = static_cast<int32_t>(line.indexOf(','));
        if (commaPos <= 0) {
            outLines.append(line);
            continue;
        }

        const QString id = line.left(commaPos).trimmed();
        if (!selections.contains(id)) {
            outLines.append(line);
            continue;
        }

        if (doneIds.value(id, false)) {
            // A second line with the same id - leave it untouched.
            if (!duplicateIds.contains(id)) {
                duplicateIds.append(id);
            }
            outLines.append(line);
            continue;
        }

        const QString newFolder = selections.value(id);
        outLines.append(id + QStringLiteral(",") + newFolder);
        doneIds.insert(id, true);
        updatedIds.append(id);
    }

    // Report any selected project that had no matching line.
    QStringList notFound;
    for (auto it = selections.constBegin();
         it != selections.constEnd(); ++it) {
        if (!doneIds.value(it.key(), false)) {
            notFound.append(it.key());
        }
    }

    QFile outFile(datPath);
    if (!outFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        *p_message = QStringLiteral("Cannot open for writing: ") + datPath;
        return false;
    }
    outFile.write(outLines.join(eol).toUtf8());
    outFile.close();

    QString msg =
        QStringLiteral("Updated %1 line(s). Backup: %2")
            .arg(updatedIds.size())
            .arg(QFileInfo(backupPath).fileName());
    if (!duplicateIds.isEmpty()) {
        msg += QStringLiteral(" | Duplicate ids left untouched: ")
               + duplicateIds.join(QStringLiteral(", "));
    }
    if (!notFound.isEmpty()) {
        msg += QStringLiteral(" | No matching line for: ")
               + notFound.join(QStringLiteral(", "));
    }
    *p_message = msg;
    return true;
}

}  // namespace prjchoosetool
