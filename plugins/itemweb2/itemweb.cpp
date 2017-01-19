/*
    Copyright (c) 2014, Lukas Holecek <hluk@email.cz>

    This file is part of CopyQ.

    CopyQ is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    CopyQ is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with CopyQ.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "itemweb.h"
#include "ui_itemwebsettings.h"

#include "common/contenttype.h"

#include <QApplication>
#include <QDesktopWidget>
#include <QDesktopServices>
#include <QModelIndex>
#include <QMouseEvent>
#include <QPalette>
#include <QtPlugin>
#include <QtWebEngineWidgets/QWebEnginePage>
#include <QtWebEngineWidgets/QWebEngineSettings>
#include <QVariant>

namespace {

const char optionMaximumHeight[] = "max_height";

bool getHtml(const QModelIndex &index, QString *text)
{
    *text = index.data(contentType::html).toString();
    if ( text->isEmpty() )
        return false;

    // Remove trailing null character.
    if ( text->endsWith(QChar(0)) )
        text->resize(text->size() - 1);

    return true;
}

bool canMouseInteract(const QInputEvent &event)
{
    return event.modifiers() & Qt::ShiftModifier;
}

} // namespace

ItemWeb::ItemWeb(const QString &html, int maximumHeight, QWidget *parent)
    : QWebEngineView(parent)
    , ItemWidget(this)
    , m_copyOnMouseUp(false)
    , m_maximumHeight(maximumHeight)
{
    //frame->setScrollBarPolicy(Qt::Horizontal, Qt::ScrollBarAlwaysOff);

    settings()->setAttribute(QWebEngineSettings::JavascriptEnabled, false);
    settings()->setAttribute(QWebEngineSettings::LocalStorageEnabled, false);

    const QFont &defaultFont = font();
    settings()->setFontFamily(QWebEngineSettings::StandardFont, defaultFont.family());
    // DPI resolution can be different than the one used by this widget.
    QWidget* window = QApplication::desktop()->screen();
    int dpi = window->logicalDpiX();
    int pt = defaultFont.pointSize();
    settings()->setFontSize(QWebEngineSettings::DefaultFontSize, pt * dpi / 72);

    QPalette pal(palette());
    pal.setBrush(QPalette::Base, Qt::transparent);
    setPalette(pal);
    setAttribute(Qt::WA_OpaquePaintEvent, false);

    setContextMenuPolicy(Qt::NoContextMenu);

    // Selecting text copies it to clipboard.
    connect( this, SIGNAL(selectionChanged()), SLOT(onSelectionChanged()) );

    // Open link with external application.
    //page()->setLinkDelegationPolicy(QWebEnginePage::DelegateAllLinks);
    //connect( page(), SIGNAL(linkClicked(QUrl)), SLOT(onLinkClicked(QUrl)) );

    setProperty("CopyQ_no_style", true);

    connect( page(), SIGNAL(loadFinished(bool)),
             this, SLOT(onItemChanged()) );

    // Set some remote URL as base URL so we can include remote scripts.
    setHtml(html, QUrl("http://example.com/"));
}

void ItemWeb::highlight(const QRegExp &re, const QFont &, const QPalette &)
{
    // FIXME: Set hightlight color and font!
    // FIXME: Hightlight text matching regular expression!
    if ( re.isEmpty() )
        findText( QString() );
    else
        findText( re.pattern() );
}

void ItemWeb::onItemChanged()
{
    updateSize(m_maximumSize, 0);
}

void ItemWeb::updateSize(const QSize &maximumSize, int)
{
    setMaximumSize(maximumSize);
    m_maximumSize = maximumSize;

    const int w = maximumSize.width();
    //const int scrollBarWidth = frame->scrollBarGeometry(Qt::Vertical).width();
    //page()->setPreferredContentsSize( QSize(w - scrollBarWidth, 10) );

    int h = sizeHint().height();
    if (0 < m_maximumHeight && m_maximumHeight < h)
        h = m_maximumHeight;

    const QSize size(w, h);
    setFixedSize(size);
}

void ItemWeb::onSelectionChanged()
{
    m_copyOnMouseUp = true;
}

void ItemWeb::onLinkClicked(const QUrl &url)
{
    if ( !QDesktopServices::openUrl(url) )
        load(url);
}

void ItemWeb::mousePressEvent(QMouseEvent *e)
{
    if ( canMouseInteract(*e) ) {
        QMouseEvent e2(QEvent::MouseButtonPress, e->pos(), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier );
        QCoreApplication::sendEvent( page(), &e2 );
        QWebEngineView::mousePressEvent(e);
        e->ignore();
    } else {
        e->ignore();
        QWebEngineView::mousePressEvent(e);
    }
}

void ItemWeb::mouseMoveEvent(QMouseEvent *e)
{
    if ( canMouseInteract(*e) )
        QWebEngineView::mousePressEvent(e);
    else
        e->ignore();
}

void ItemWeb::wheelEvent(QWheelEvent *e)
{
    if ( canMouseInteract(*e) )
        QWebEngineView::wheelEvent(e);
    else
        e->ignore();
}

void ItemWeb::mouseReleaseEvent(QMouseEvent *e)
{
    if (m_copyOnMouseUp) {
        m_copyOnMouseUp = false;
#if QT_VERSION >= 0x040800
        if ( hasSelection() )
#endif
            triggerPageAction(QWebEnginePage::Copy);
    } else {
        QWebEngineView::mouseReleaseEvent(e);
    }
}

void ItemWeb::mouseDoubleClickEvent(QMouseEvent *e)
{
    if ( canMouseInteract(*e) )
        QWebEngineView::mouseDoubleClickEvent(e);
    else
        e->ignore();
}

ItemWebLoader::ItemWebLoader()
{
}

ItemWebLoader::~ItemWebLoader()
{
}

ItemWidget *ItemWebLoader::create(const QModelIndex &index, QWidget *parent) const
{
    if ( index.data(contentType::isHidden).toBool() )
        return NULL;

    QString html;
    if ( getHtml(index, &html) )
        return new ItemWeb(html, m_settings.value(optionMaximumHeight, 0).toInt(), parent);

    return NULL;
}

QStringList ItemWebLoader::formatsToSave() const
{
    return QStringList("text/plain") << QString("text/html");
}

QVariantMap ItemWebLoader::applySettings()
{
    m_settings[optionMaximumHeight] = ui->spinBoxMaxHeight->value();
    return m_settings;
}

QWidget *ItemWebLoader::createSettingsWidget(QWidget *parent)
{
    ui.reset(new Ui::ItemWebSettings);
    QWidget *w = new QWidget(parent);
    ui->setupUi(w);
    ui->spinBoxMaxHeight->setValue( m_settings.value(optionMaximumHeight, 0).toInt() );
    return w;
}

Q_EXPORT_PLUGIN2(itemweb, ItemWebLoader)