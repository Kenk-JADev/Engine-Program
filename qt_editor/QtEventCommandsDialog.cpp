#include "QtEventCommandsDialog.h"
#include "QtEventCommandCatalog.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QListWidget>
#include <QPushButton>
#include <QTabWidget>
#include <QVBoxLayout>

namespace qt_editor {

QtEventCommandsDialog::QtEventCommandsDialog(QWidget* parent)
    : QDialog(parent) {
    setWindowTitle(QStringLiteral("Event-Befehle"));
    setModal(true);
    resize(430, 560);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(6, 6, 6, 6);

    mTabs = new QTabWidget(this);
    mTabs->setTabPosition(QTabWidget::North);
    root->addWidget(mTabs, 1);

    // XP: drei Seiten; jede Seite eine vertikale Button-Liste (QListWidget
    // verhaelt sich wie die XP-Buttons: einfach waehlen, doppelklick = OK).
    for (int page = 1; page <= 3; ++page) {
        auto* list = new QListWidget(mTabs);
        list->setUniformItemSizes(true);
        list->setSelectionMode(QAbstractItemView::SingleSelection);
        list->setProperty("page", page);
        for (const CommandSpec& spec : GetEventCommandCatalog()) {
            if (spec.structural || spec.page != page) continue;
            auto* item = new QListWidgetItem(spec.label, list);
            item->setData(Qt::UserRole, static_cast<int>(spec.code));
            item->setToolTip(spec.description);
        }
        // Separator-Eintraege wie in XP (optische Gruppierung)
        connect(list, &QListWidget::currentRowChanged,
                this, &QtEventCommandsDialog::onSelectionChanged);
        connect(list, &QListWidget::itemDoubleClicked,
                this, &QtEventCommandsDialog::onDoubleClicked);
        mTabs->addTab(list, QString::number(page));
    }
    connect(mTabs, &QTabWidget::currentChanged,
            this, &QtEventCommandsDialog::onSelectionChanged);

    if (auto* first = mTabs->widget(0))
        if (auto* l = qobject_cast<QListWidget*>(first))
            if (l->count() > 0) l->setCurrentRow(0);

    auto* buttons = new QDialogButtonBox(this);
    mOkButton = buttons->addButton(QDialogButtonBox::Ok);
    mOkButton->setText(QStringLiteral("OK"));
    auto* cancel = buttons->addButton(QDialogButtonBox::Cancel);
    cancel->setText(QStringLiteral("Abbrechen"));
    connect(mOkButton, &QPushButton::clicked, this, &QtEventCommandsDialog::onOk);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    root->addWidget(buttons);
}

void QtEventCommandsDialog::onSelectionChanged() {
    auto* list = qobject_cast<QListWidget*>(mTabs->currentWidget());
    QListWidgetItem* item = list ? list->currentItem() : nullptr;
    mSelected = item ? static_cast<rpg::EventCommandCode>(item->data(Qt::UserRole).toInt())
                     : rpg::EventCommandCode::None;
    if (mOkButton) mOkButton->setEnabled(item != nullptr);
}

void QtEventCommandsDialog::onDoubleClicked() {
    onSelectionChanged();
    if (mSelected != rpg::EventCommandCode::None) accept();
}

void QtEventCommandsDialog::onOk() {
    onSelectionChanged();
    if (mSelected == rpg::EventCommandCode::None) return;
    accept();
}

bool QtEventCommandsDialog::ChooseCommand(QWidget* parent, rpg::EventCommandCode& outCode) {
    QtEventCommandsDialog dlg(parent);
    if (dlg.exec() != QDialog::Accepted) return false;
    outCode = dlg.selectedCode();
    return outCode != rpg::EventCommandCode::None;
}

} // namespace qt_editor
