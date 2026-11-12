///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Some code by AI                                                               //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
// (at your option) any later version.                                           //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#ifndef PLUGINS_CHANNELTX_MODDSE_DSEMODAPRSSYMBOLDIALOG_H_
#define PLUGINS_CHANNELTX_MODDSE_DSEMODAPRSSYMBOLDIALOG_H_

#include <QDialog>
#include <QString>

class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QTabWidget;

/**
 * Picks an APRS symbol (ANGEL61.md 3.3) from the two symbol tables shown as grids of their
 * icons, rather than from the two characters that name it.
 *
 * The icons are the APRS feature's: it holds the whole set as a Qt resource under /aprs/, which
 * every loaded plugin can read. Built without that feature, the grids fall back to the symbol
 * characters, so the dialog still picks a symbol, just without the pictures.
 *
 * The alternate table takes an overlay character in place of its table character, which is how
 * a symbol is marked with a digit or a letter (a "1" ambulance, a "W" water station).
 */
class DSEModAPRSSymbolDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DSEModAPRSSymbolDialog(const QString& symbol, QWidget *parent = nullptr);

    //! The two characters chosen: the table (or the overlay) and the code
    QString symbol() const { return m_symbol; }

    //! The icon of a symbol, or a null pixmap when the APRS feature's resources are not there
    static QPixmap symbolPixmap(const QString& symbol);

    static constexpr int NbSymbols = 94;   //!< the codes '!' to '~' of each table

private slots:
    void tableChanged(int index);
    void symbolClicked(QListWidgetItem *item);
    void overlayChanged(const QString& text);

private:
    QListWidget *table(int index) const;
    void select(char symbolTable, char symbolCode);

    QTabWidget *m_tables;
    QListWidget *m_primary;
    QListWidget *m_alternate;
    QLineEdit *m_overlay;
    QLabel *m_overlayLabel;
    QString m_symbol;
};

#endif // PLUGINS_CHANNELTX_MODDSE_DSEMODAPRSSYMBOLDIALOG_H_
