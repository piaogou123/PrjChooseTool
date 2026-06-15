/*
 * @CopyRight: iNovatrol
 * @Description: Implementation of the project-variant chooser window
 * @version: <SET-YOUR-INITIALS> <SET-YOUR-VERSION>   // e.g. JCK J01
 * @Author: <SET-YOUR-NAME-IN-PINYIN>
 * @Date: 2026.06.15
 */

#include "mainwindow.h"   // HD-10: own header first

#include <cstdint>

#include <QButtonGroup>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QSettings>
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

// Property key used to stash the full folder name on each radio button.
const char* const FOLDER_PROPERTY  = "folderName";
// Property key used to stash the project id on each project option.
const char* const PROJECT_PROPERTY = "projectId";

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
    OnReload();
}

MainWindow::~MainWindow()
{
    // Child widgets and button groups are owned by Qt's parent hierarchy,
    // so no manual cleanup is required here.
}

void MainWindow::BuildUi()
{
    setWindowTitle(QStringLiteral("Project Chooser - ProjectDefinition.dat"));
    resize(560, 520);

    QWidget* p_central = new QWidget(this);
    setCentralWidget(p_central);

    QVBoxLayout* p_mainLayout = new QVBoxLayout(p_central);

    // --- Top row: working directory + Browse + Reload ---------------
    QHBoxLayout* p_topRow = new QHBoxLayout();

    QLabel* p_dirLabel = new QLabel(QStringLiteral("Working folder:"),
                                    p_central);

    // First launch: empty. Otherwise reuse the last folder the user picked.
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

    // --- Horizontal project selector -------------------------------
    QLabel* p_projectLabel = new QLabel(QStringLiteral("Project:"),
                                        p_central);
    p_mainLayout->addWidget(p_projectLabel);

    m_pProjectScroll = new QScrollArea(p_central);
    m_pProjectScroll->setWidgetResizable(true);
    m_pProjectScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_pProjectScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_pProjectScroll->setFixedHeight(56);
    p_mainLayout->addWidget(m_pProjectScroll);

    // --- Variant list of the chosen project ------------------------
    m_pVariantTitle = new QLabel(QStringLiteral("Variants:"), p_central);
    p_mainLayout->addWidget(m_pVariantTitle);

    m_pVariantScroll = new QScrollArea(p_central);
    m_pVariantScroll->setWidgetResizable(true);
    p_mainLayout->addWidget(m_pVariantScroll, 1);

    // --- Bottom row: status label + Apply --------------------------
    QHBoxLayout* p_bottomRow = new QHBoxLayout();

    m_pStatusLabel = new QLabel(QString(), p_central);
    m_pStatusLabel->setWordWrap(true);

    QPushButton* p_applyButton =
        new QPushButton(QStringLiteral("Apply selection"), p_central);

    p_bottomRow->addWidget(m_pStatusLabel, 1);
    p_bottomRow->addWidget(p_applyButton);

    p_mainLayout->addLayout(p_bottomRow);

    connect(p_browseButton, &QPushButton::clicked,
            this, &MainWindow::OnBrowse);
    connect(p_reloadButton, &QPushButton::clicked,
            this, &MainWindow::OnReload);
    connect(p_applyButton, &QPushButton::clicked,
            this, &MainWindow::OnApply);
}

QString MainWindow::DatFilePath() const
{
    const QString baseDir = m_pBaseDirEdit->text().trimmed();
    return QDir(baseDir).filePath(
        DEFINITION_SUBDIR + "/" + DEFINITION_FILE);
}

void MainWindow::SetStatus(const QString& text)
{
    if (m_pStatusLabel != nullptr) {
        m_pStatusLabel->setText(text);
    }
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
        SetStatus(QStringLiteral(
            "Please select a working folder (Browse...)."));
        PopulateProjectBar();
        PopulateVariantPanel(QString());
        return;
    }
    if (!QDir(baseDir).exists()) {
        SetStatus(QStringLiteral("Working folder does not exist: ") + baseDir);
        PopulateProjectBar();
        PopulateVariantPanel(QString());
        return;
    }

    // Remember this folder for next launch.
    QSettings settings;
    settings.setValue(SETTINGS_LAST_DIR, baseDir);

    ScanProjects(baseDir, &m_projects);

    QStringList duplicateIds;
    const bool datOk =
        LoadActiveVariants(DatFilePath(), &m_activeVariants, &duplicateIds);

    PopulateProjectBar();
    PopulateVariantPanel(QString());   // nothing chosen yet

    QString status =
        QStringLiteral("Found %1 project(s).").arg(m_projects.size());
    if (!datOk) {
        status += QStringLiteral(" (ProjectDefinition.dat not found at ")
                  + DatFilePath() + QStringLiteral(")");
    }
    if (!duplicateIds.isEmpty()) {
        status += QStringLiteral(" Duplicate ids in .dat: ")
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

void MainWindow::PopulateProjectBar()
{
    // Replacing the content widget deletes the previous project options
    // and their button group.
    QWidget* p_bar = new QWidget();
    QHBoxLayout* p_barLayout = new QHBoxLayout(p_bar);
    p_barLayout->setContentsMargins(4, 4, 4, 4);

    m_pProjectGroup = new QButtonGroup(p_bar);
    m_pProjectGroup->setExclusive(true);

    for (auto it = m_projects.constBegin();
         it != m_projects.constEnd(); ++it) {
        const QString& projectId = it.key();

        QRadioButton* p_option = new QRadioButton(projectId, p_bar);
        p_option->setProperty(PROJECT_PROPERTY, projectId);
        m_pProjectGroup->addButton(p_option);
        p_barLayout->addWidget(p_option);

        connect(p_option, &QRadioButton::toggled,
                this, &MainWindow::OnProjectSelected);
    }

    p_barLayout->addStretch(1);
    m_pProjectScroll->setWidget(p_bar);
}

void MainWindow::OnProjectSelected()
{
    if (m_pProjectGroup == nullptr) {
        return;
    }
    const QAbstractButton* p_checked = m_pProjectGroup->checkedButton();
    if (p_checked == nullptr) {
        return;
    }
    const QString projectId =
        p_checked->property(PROJECT_PROPERTY).toString();
    PopulateVariantPanel(projectId);
}

void MainWindow::PopulateVariantPanel(const QString& projectId)
{
    // Fresh content widget; the old variant group is deleted with it.
    QWidget* p_panel = new QWidget();
    QVBoxLayout* p_panelLayout = new QVBoxLayout(p_panel);

    if (projectId.isEmpty()) {
        m_pVariantTitle->setText(
            QStringLiteral("Variants: (select a project above)"));
        m_pVariantGroup = nullptr;
        p_panelLayout->addStretch(1);
        m_pVariantScroll->setWidget(p_panel);
        return;
    }

    m_pVariantTitle->setText(
        QStringLiteral("Variants of %1:").arg(projectId));

    m_pVariantGroup = new QButtonGroup(p_panel);
    m_pVariantGroup->setExclusive(true);

    const ProjectInfo info = m_projects.value(projectId);
    const QString activeFolder = m_activeVariants.value(projectId);
    bool activeMatched = false;

    for (const QString& folder : info.variantFolders) {
        QRadioButton* p_radio = new QRadioButton(folder, p_panel);
        p_radio->setProperty(FOLDER_PROPERTY, folder);
        m_pVariantGroup->addButton(p_radio);
        p_panelLayout->addWidget(p_radio);

        if (folder == activeFolder) {
            p_radio->setChecked(true);
            activeMatched = true;
        }
    }

    if (!activeFolder.isEmpty() && !activeMatched) {
        QLabel* p_note = new QLabel(
            QStringLiteral("Current .dat value not found on disk: ")
            + activeFolder,
            p_panel);
        p_note->setWordWrap(true);
        p_panelLayout->addWidget(p_note);
    }

    p_panelLayout->addStretch(1);
    m_pVariantScroll->setWidget(p_panel);
}

void MainWindow::OnApply()
{
    if (m_pProjectGroup == nullptr || m_projects.isEmpty()) {
        SetStatus(QStringLiteral("Nothing to apply - no projects loaded."));
        return;
    }

    const QAbstractButton* p_project = m_pProjectGroup->checkedButton();
    if (p_project == nullptr) {
        SetStatus(QStringLiteral("Select a project before applying."));
        return;
    }
    const QString projectId =
        p_project->property(PROJECT_PROPERTY).toString();

    if (m_pVariantGroup == nullptr) {
        SetStatus(QStringLiteral("Select a variant before applying."));
        return;
    }
    const QAbstractButton* p_variant = m_pVariantGroup->checkedButton();
    if (p_variant == nullptr) {
        SetStatus(QStringLiteral("Select a variant for project ")
                  + projectId + QStringLiteral(" before applying."));
        return;
    }
    const QString folder = p_variant->property(FOLDER_PROPERTY).toString();
    if (folder.isEmpty()) {
        SetStatus(QStringLiteral("Selected variant is invalid."));
        return;
    }

    QMap<QString, QString> selections;
    selections.insert(projectId, folder);

    QString message;
    const bool ok = WriteDefinitions(DatFilePath(), selections, &message);

    if (ok) {
        QMessageBox::information(
            this, QStringLiteral("ProjectDefinition.dat updated"), message);
    } else {
        QMessageBox::warning(this, QStringLiteral("Update failed"), message);
    }
    SetStatus(message);

    // Reload so the displayed state matches the freshly written file, then
    // restore the project the user was working on.
    OnReload();
    if (m_pProjectGroup != nullptr) {
        const QList<QAbstractButton*> buttons = m_pProjectGroup->buttons();
        for (QAbstractButton* p_button : buttons) {
            if (p_button != nullptr
                && p_button->property(PROJECT_PROPERTY).toString()
                       == projectId) {
                p_button->setChecked(true);
                break;
            }
        }
    }
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
