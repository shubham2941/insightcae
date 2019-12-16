/*
 * This file is part of Insight CAE, a workbench for Computer-Aided Engineering
 * Copyright (C) 2014  Hannes Kroeger <hannes@kroegeronline.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */


#include <QDoubleSpinBox>

#include "isofplottabularwindow.h"
#include "plotwidget.h"

#include "base/tools.h"
#include "base/exception.h"

#include "openfoam/openfoamtools.h"

#include "boost/algorithm/string/trim.hpp"
#include <boost/algorithm/string.hpp>

#include "base/qt5_helper.h"

using namespace std;
using namespace boost;

IsofPlotTabularWindow::IsofPlotTabularWindow(const boost::filesystem::path& file)
  : QMainWindow(),
    file_(file)
{
  ui=new Ui_MainWindow;
  ui->setupUi(this);

  setWindowTitle( "PlotTabularData: CWD "+QString::fromStdString( boost::filesystem::current_path().string() )
                  +", displaying "+ QString::fromStdString( file.string() ) );

  connect(ui->updateBtn, &QPushButton::clicked,
          this, &IsofPlotTabularWindow::onUpdate);

  onUpdate();
}


void IsofPlotTabularWindow::onUpdate(bool)
{
  std::istream* f;
  std::shared_ptr<std::ifstream> fs;

  if (file_.string()=="-")
  {
    f=&std::cin;
  }
  else
  {
    fs.reset(new ifstream(file_.c_str()));
    f=fs.get();
  }

  data_ = insight::readTextFile(*f);

  if (data_.n_rows!=0)
  {
    int n_cols=data_.n_cols-1; // first col is time

    // remove unnecessary tabs
    for (int j=ui->graphs->count()-1; j>=n_cols; j--)
    {
      ui->graphs->removeTab(j);
    }

    // add new tabs, if required
    for (int j=ui->graphs->count(); j<n_cols; j++)
    {
      PlotWidget* pw=new PlotWidget(this);
      ui->graphs->addTab(pw,
                         QString::fromStdString(str(format("Col %d")%j))
                         );
      connect(ui->startTime, &QLineEdit::textChanged,
              [=](const QString& nv) { pw->onChangeXRange(nv, ui->endTime->text()); } );
      connect(ui->endTime, &QLineEdit::textChanged,
              [=](const QString& nv) { pw->onChangeXRange(ui->startTime->text(), nv); } );

    }

    for (size_t j=1; j<data_.n_cols; j++)
    {
      PlotWidget *p = dynamic_cast<PlotWidget*>(ui->graphs->widget(j-1));
      p->setData(data_.col(0), data_.col(j));
    }

    if (ui->graphs->count()!=0)
    {
      if (PlotWidget *pw = dynamic_cast<PlotWidget*>(ui->graphs->currentWidget()))
      {
        pw->onShow();
      }
    }
  }

  connect(ui->graphs, &QTabWidget::currentChanged, this, &IsofPlotTabularWindow::onTabChanged);
}

void IsofPlotTabularWindow::onTabChanged(int ct)
{
  if (PlotWidget* pw = dynamic_cast<PlotWidget*>(ui->graphs->widget(ct)))
  {
//    qDebug()<<"ct="<<ct;
    pw->onShow();
  }
}
