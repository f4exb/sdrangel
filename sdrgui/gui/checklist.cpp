#include "checklist.h"

CheckList::CheckList(QWidget* parent) :
    QComboBox(parent)
{
    m_model = new QStandardItemModel();
    setModel(m_model);

    setEditable(true);
    lineEdit()->setReadOnly(true);
    lineEdit()->installEventFilter(this);
    setItemDelegate(new QCheckListStyledItemDelegate(this));

    connect(lineEdit(), &QLineEdit::selectionChanged, lineEdit(), &QLineEdit::deselect);
    connect((QListView*) view(), SIGNAL(pressed(QModelIndex)), this, SLOT(on_itemPressed(QModelIndex)));
    connect(m_model, SIGNAL(dataChanged(QModelIndex, QModelIndex, QVector<int>)), this, SLOT(on_modelDataChanged()));
}

CheckList::~CheckList()
{
    delete m_model;
}

void CheckList::setText(const QString &text)
{
    m_text = text;
    updateText();
}

void CheckList::setAllCheckedText(const QString &text)
{
    m_allCheckedText = text;
    updateText();
}

void CheckList::setNoneCheckedText(const QString &text)
{
    m_noneCheckedText = text;
    updateText();
}

void CheckList::setUnknownlyCheckedText(const QString &text)
{
    m_unknownlyCheckedText = text;
    updateText();
}

void CheckList::setSortRole(int role)
{
    m_model->setSortRole(role);
}

QStandardItem* CheckList::addCheckItem(const QString &label, const QVariant &data, const Qt::CheckState checkState)
{
    QStandardItem* item = new QStandardItem(label);
    item->setCheckState(checkState);
    item->setData(data);
    item->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);

    m_model->appendRow(item);

    updateText();

    return item;
}

int CheckList::globalCheckState()
{
    int nbRows = m_model->rowCount(), nbChecked = 0, nbUnchecked = 0;

    if (nbRows == 0)
    {
        return StateUnknown;
    }

    for (int i = 0; i < nbRows; i++)
    {
        if (m_model->item(i)->checkState() == Qt::Checked)
        {
            nbChecked++;
        }
        else if (m_model->item(i)->checkState() == Qt::Unchecked)
        {
            nbUnchecked++;
        }
        else
        {
            return StateUnknown;
        }
    }

    return nbChecked == nbRows ? Qt::Checked : nbUnchecked == nbRows ? Qt::Unchecked : Qt::PartiallyChecked;
}

bool CheckList::eventFilter(QObject* _object, QEvent* _event)
{
    if (_object == lineEdit() && _event->type() == QEvent::MouseButtonPress)
    {
        showPopup();
        return true;
    }

    return false;
}

void CheckList::updateText()
{
    QString text;

    if (m_text.isEmpty())
    {
        switch (globalCheckState())
        {
        case Qt::Checked:
            text = m_allCheckedText;
            break;

        case Qt::Unchecked:
            text = m_noneCheckedText;
            break;

        case Qt::PartiallyChecked:
            for (int i = 0; i < m_model->rowCount(); i++)
            {
                if (m_model->item(i)->checkState() == Qt::Checked)
                {
                    if (!text.isEmpty())
                    {
                        text+= ", ";
                    }

                    text+= m_model->item(i)->text();
                }
            }
            break;

        default:
            text = m_unknownlyCheckedText;
        }
    }
    else
    {
        text = m_text;
    }

    lineEdit()->setText(text);
}

void CheckList::on_modelDataChanged()
{
    updateText();
    emit globalCheckStateChanged(globalCheckState());
}

void CheckList::on_itemPressed(const QModelIndex &index)
{
    QStandardItem* item = m_model->itemFromIndex(index);

    if (item->checkState() == Qt::Checked)
    {
        item->setCheckState(Qt::Unchecked);
    }
    else
    {
        item->setCheckState(Qt::Checked);
    }
}

bool CheckList::isChecked(int index) const
{
    return m_model->item(index)->checkState() == Qt::Checked;
}
