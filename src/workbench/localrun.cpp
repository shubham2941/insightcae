#include "localrun.h"
#include "analysisform.h"
#include "ui_analysisform.h"
#include "progressrelay.h"

#include <QMessageBox>




LocalRun::LocalRun(AnalysisForm *af)
  : WorkbenchAction(af),
    analysis_(
          insight::Analysis::lookup(
            af_->analysisName_,
            af_->parameters_,
            *(af_->caseDirectory_)
            )
          ),
    workerThread_(analysis_, &af->progressDisplayer_)
{
  connectAnalysisThread(&workerThread_);
  af_->progressDisplayer_.reset();
  af_->ui->tabWidget->setCurrentWidget(af_->ui->runTab);
}



LocalRun::~LocalRun()
{
  workerThread_.join();
}


void LocalRun::onCancel()
{
  workerThread_.interrupt();
}
