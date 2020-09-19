///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Edouard Griffiths, F4EXB                                   //
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

#include "gui/featurewindow.h"
#include "plugin/plugininstancegui.h"

#include "featureuiset.h"

FeatureUISet::FeatureUISet(int tabIndex)
{
    m_featureWindow = new FeatureWindow;
    m_featureTabIndex = tabIndex;
}

FeatureUISet::~FeatureUISet()
{
    delete m_featureWindow;
}

void FeatureUISet::addRollupWidget(QWidget *widget)
{
    m_featureWindow->addRollupWidget(widget);
}

void FeatureUISet::registerFeatureInstance(const QString& featureName, PluginInstanceGUI* pluginGUI)
{
    m_featureInstanceRegistrations.append(FeatureInstanceRegistration(featureName, pluginGUI));
    renameFeatureInstances();
}

void FeatureUISet::removeFeatureInstance(PluginInstanceGUI* pluginGUI)
{
    for (FeatureInstanceRegistrations::iterator it = m_featureInstanceRegistrations.begin(); it != m_featureInstanceRegistrations.end(); ++it)
    {
        if (it->m_gui == pluginGUI)
        {
            m_featureInstanceRegistrations.erase(it);
            break;
        }
    }

    renameFeatureInstances();
}

void FeatureUISet::renameFeatureInstances()
{
    for (int i = 0; i < m_featureInstanceRegistrations.count(); i++) {
        m_featureInstanceRegistrations[i].m_gui->setName(QString("%1:%2").arg(m_featureInstanceRegistrations[i].m_featureName).arg(i));
    }
}

void FeatureUISet::freeFeatures()
{
    for(int i = 0; i < m_featureInstanceRegistrations.count(); i++)
    {
        qDebug("FeatureUISet::freeFeatures: destroying feature [%s]", qPrintable(m_featureInstanceRegistrations[i].m_featureName));
        m_featureInstanceRegistrations[i].m_gui->destroy();
    }
}

void FeatureUISet::deleteFeature(int featureIndex)
{
    if ((featureIndex >= 0) && (featureIndex < m_featureInstanceRegistrations.count()))
    {
        qDebug("FeatureUISet::deleteFeature: delete feature [%s] at %d",
                qPrintable(m_featureInstanceRegistrations[featureIndex].m_featureName),
                featureIndex);
        m_featureInstanceRegistrations[featureIndex].m_gui->destroy();
        renameFeatureInstances();
    }
}
