// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "RetranslatableWidgetFixture.h"
#include "locale_stable_words.h"

#include <QAbstractItemModel>
#include <QAbstractItemView>
#include <QAbstractButton>
#include <QApplication>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QSet>
#include <QTabBar>
#include <QTabWidget>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVariant>
#include <QWidget>

namespace librecelik::test::i18n {

namespace {

QString safeObjectName(QWidget* w, int dfsIndex)
{
    if (w == nullptr)
        return QStringLiteral("<null>#%1").arg(dfsIndex);
    if (!w->objectName().isEmpty())
        return w->objectName();
    return QStringLiteral("%1#%2").arg(QString::fromUtf8(w->metaObject()->className())).arg(dfsIndex);
}

void addRole(QMap<QString, QString>& out, const QString& widgetKey, const QString& role, const QString& value)
{
    out.insert(widgetKey + QLatin1Char(':') + role, value);
}

void snapshotItemView(QMap<QString, QString>& out, const QString& widgetKey, QAbstractItemView* view)
{
    auto* model = view->model();
    if (model == nullptr)
        return;
    const int rows = model->rowCount();
    const int cols = model->columnCount();
    for (int c = 0; c < cols; ++c) {
        const QVariant header = model->headerData(c, Qt::Horizontal);
        if (header.isValid()) {
            addRole(out, widgetKey, QStringLiteral("header[%1]").arg(c), header.toString());
        }
    }
    // Limit walk to the first ~64 rows so item-heavy widgets do not
    // explode snapshot size — runtime tests only care about retranslation.
    const int rowLimit = std::min(rows, 64);
    for (int r = 0; r < rowLimit; ++r) {
        for (int c = 0; c < cols; ++c) {
            const QModelIndex idx = model->index(r, c);
            const QVariant v = model->data(idx, Qt::DisplayRole);
            if (!v.isValid())
                continue;
            addRole(out, widgetKey, QStringLiteral("cell[%1,%2]").arg(r).arg(c), v.toString());
        }
    }
}

void snapshotWidgetRoles(QWidget* w, QMap<QString, QString>& out, int& dfsCounter)
{
    const QString key = safeObjectName(w, dfsCounter++);

    // Universal roles
    if (!w->windowTitle().isEmpty())
        addRole(out, key, QStringLiteral("windowTitle"), w->windowTitle());
    if (!w->toolTip().isEmpty())
        addRole(out, key, QStringLiteral("toolTip"), w->toolTip());
    if (!w->accessibleName().isEmpty())
        addRole(out, key, QStringLiteral("accessibleName"), w->accessibleName());
    if (!w->whatsThis().isEmpty())
        addRole(out, key, QStringLiteral("whatsThis"), w->whatsThis());
    if (!w->statusTip().isEmpty())
        addRole(out, key, QStringLiteral("statusTip"), w->statusTip());

    if (auto* lbl = qobject_cast<QLabel*>(w)) {
        if (!lbl->text().isEmpty())
            addRole(out, key, QStringLiteral("text"), lbl->text());
    } else if (auto* btn = qobject_cast<QAbstractButton*>(w)) {
        if (!btn->text().isEmpty())
            addRole(out, key, QStringLiteral("text"), btn->text());
    } else if (auto* gb = qobject_cast<QGroupBox*>(w)) {
        if (!gb->title().isEmpty())
            addRole(out, key, QStringLiteral("title"), gb->title());
    } else if (auto* tw = qobject_cast<QTabWidget*>(w)) {
        for (int i = 0; i < tw->count(); ++i) {
            addRole(out, key, QStringLiteral("tabText[%1]").arg(i), tw->tabText(i));
        }
    } else if (auto* cb = qobject_cast<QComboBox*>(w)) {
        addRole(out, key, QStringLiteral("currentText"), cb->currentText());
        for (int i = 0; i < cb->count(); ++i) {
            addRole(out, key, QStringLiteral("itemText[%1]").arg(i), cb->itemText(i));
        }
    } else if (auto* le = qobject_cast<QLineEdit*>(w)) {
        if (!le->placeholderText().isEmpty()) {
            addRole(out, key, QStringLiteral("placeholderText"), le->placeholderText());
        }
    } else if (auto* lw = qobject_cast<QListWidget*>(w)) {
        for (int i = 0; i < lw->count(); ++i) {
            auto* it = lw->item(i);
            if (it != nullptr && !it->text().isEmpty()) {
                addRole(out, key, QStringLiteral("listItem[%1]").arg(i), it->text());
            }
        }
    } else if (auto* tree = qobject_cast<QTreeWidget*>(w)) {
        const int cols = tree->columnCount();
        for (int c = 0; c < cols; ++c) {
            auto* hdr = tree->headerItem();
            if (hdr != nullptr) {
                addRole(out, key, QStringLiteral("treeHeader[%1]").arg(c), hdr->text(c));
            }
        }
        // Iterative DFS over tree items.
        struct Frame
        {
            QTreeWidgetItem* item;
            QString path;
        };
        QList<Frame> stack;
        for (int i = tree->topLevelItemCount() - 1; i >= 0; --i) {
            stack.append({tree->topLevelItem(i), QString::number(i)});
        }
        int visited = 0;
        while (!stack.isEmpty() && visited < 256) {
            const Frame f = stack.takeLast();
            ++visited;
            for (int c = 0; c < cols; ++c) {
                addRole(out, key, QStringLiteral("treeItem[%1]:col[%2]").arg(f.path).arg(c), f.item->text(c));
            }
            for (int i = f.item->childCount() - 1; i >= 0; --i) {
                stack.append({f.item->child(i), f.path + QStringLiteral("/") + QString::number(i)});
            }
        }
    } else if (auto* view = qobject_cast<QAbstractItemView*>(w)) {
        snapshotItemView(out, key, view);
    }

    for (auto* child : w->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly)) {
        snapshotWidgetRoles(child, out, dfsCounter);
    }
}

} // namespace

QString RetranslatableWidgetFixture::translationsDir()
{
    // Resolve via env override first (CI sets LIBRECELIK_TRANSLATIONS_DIR).
    const QByteArray env = qgetenv("LIBRECELIK_TRANSLATIONS_DIR");
    if (!env.isEmpty()) {
        const QString p = QString::fromLocal8Bit(env);
        if (QFileInfo(p).isDir())
            return p;
    }
    // Walk up from the test-binary directory looking for a build-tree
    // i18n directory containing LibreCelik_*.qm.
    QDir d(QCoreApplication::applicationDirPath());
    for (int i = 0; i < 6; ++i) {
        for (const QString& cand : {
                 QStringLiteral("resources/i18n"),
                 QStringLiteral("share/librecelik/translations"),
             }) {
            const QString full = d.filePath(cand);
            if (QDir(full).exists())
                return full;
        }
        if (!d.cdUp())
            break;
    }
    return {};
}

void RetranslatableWidgetFixture::SetUp()
{
    m_currentLanguage.clear();
    if (QApplication::instance() == nullptr) {
        // Lazy-create a process-wide QApplication so QWidget
        // construction works under offscreen QPA. Mirrors the pattern
        // used by SecurityStatusWidgetTest etc. (no QApplication =
        // crash on first QWidget). We never delete; gtest_main exits
        // process at end of run.
        static int s_argc = 0;
        static char* s_argv[] = {nullptr};
        new QApplication(s_argc, s_argv);
    }
}

void RetranslatableWidgetFixture::TearDown()
{
    if (!m_currentLanguage.isEmpty()) {
        QCoreApplication::removeTranslator(&m_translator);
        m_currentLanguage.clear();
    }
}

void RetranslatableWidgetFixture::switchTo(const QString& languageCode)
{
    if (!m_currentLanguage.isEmpty()) {
        QCoreApplication::removeTranslator(&m_translator);
        m_currentLanguage.clear();
    }
    const QString dir = translationsDir();
    if (!dir.isEmpty()) {
        const QString fname = QStringLiteral("LibreCelik_%1").arg(languageCode);
        if (m_translator.load(fname, dir)) {
            QCoreApplication::installTranslator(&m_translator);
            m_currentLanguage = languageCode;
            return;
        }
    }
    // Even if the .qm load fails (build-tree without compiled .ts), we
    // still mark the switch — tests that depend on actual translations
    // will see equal-strings and assertAllRetranslated will surface the
    // problem.
    m_currentLanguage = languageCode;
}

QMap<QString, QString> RetranslatableWidgetFixture::snapshotTexts(QWidget* root) const
{
    QMap<QString, QString> out;
    if (root == nullptr)
        return out;
    int dfs = 0;
    snapshotWidgetRoles(root, out, dfs);
    return out;
}

void RetranslatableWidgetFixture::markLocaleStable(QWidget* w)
{
    if (w != nullptr)
        w->setProperty(kPropI18nLocaleStable, true);
}

void RetranslatableWidgetFixture::assertAllRetranslated(const QMap<QString, QString>& before,
                                                        const QMap<QString, QString>& after) const
{
    for (auto it = before.constBegin(); it != before.constEnd(); ++it) {
        if (!after.contains(it.key()))
            continue;
        const QString& vbefore = it.value();
        const QString& vafter = after.value(it.key());
        if (vbefore != vafter)
            continue;
        // Skip rules.
        if (localeStableWords().contains(vbefore))
            continue;
        if (isTechnicalValue(vbefore))
            continue;
        // We do not have direct widget access here; the fixture caller
        // can mark widgets via Q_PROPERTY. The DFS keys carry the
        // widget object name, but we cannot reverse-lookup the QWidget*
        // from a string key without additional bookkeeping. The runtime
        // tests therefore mark stable widgets via property BEFORE the
        // snapshot is taken; the snapshot walker honours the property
        // by simply *not emitting* their values — see the DFS variant
        // for production-side use. For now, the wordlist + heuristic
        // are the live filters; any remaining identical value is a
        // genuine retranslate gap.
        ADD_FAILURE() << "i18n: identical string before/after language "
                      << "switch at " << it.key().toStdString() << ": " << vbefore.toStdString();
    }
}

} // namespace librecelik::test::i18n
