
#include "analysisform.h"
#include "ui_analysisform.h"

#include "remotesync.h"

#include "localrun.h"
#ifdef HAVE_WT
#include "remoterun.h"
#endif

bool AnalysisForm::isRunningLocally() const
{
  if (currentWorkbenchAction_)
  {
    return dynamic_cast<LocalRun*>(currentWorkbenchAction_.get());
  }

  return false;
}




bool AnalysisForm::isRunningRemotely() const
{
#ifdef HAVE_WT
  if (currentWorkbenchAction_)
  {
    return dynamic_cast<RemoteRun*>(currentWorkbenchAction_.get());
  }
#endif

  return false;
}




bool AnalysisForm::isRunning() const
{
  return isRunningLocally()||isRunningRemotely();
}




bool AnalysisForm::hasValidExecutionPath() const
{
  return ui->localDir->text().isEmpty() || boost::filesystem::exists( ui->localDir->text().toStdString() );
}




bool AnalysisForm::remoteDownloadOrResumeIsPossible() const
{
  return
      (ui->cbRemoteRun->checkState()==Qt::Checked)
      &&
      (!ui->remoteDir->text().isEmpty())
      &&
      (!ui->localDir->text().isEmpty())
      &&
      isRemoteDirectoryPresent()
      ;
}




bool AnalysisForm::isRemoteDirectoryPresent() const
{
  try
  {
    insight::RemoteExecutionConfig ec(currentExecutionPath(false));
    return ec.remoteDirExists();
  }
  catch (...) {}

  return false;
}




boost::filesystem::path AnalysisForm::currentExecutionPath(bool createIfNonexistent) const
{
  boost::filesystem::path ep;

  QString setPath = ui->localDir->text();
  if (setPath.isEmpty())
  {
    if (createIfNonexistent)
    {
      std::unique_ptr<insight::Analysis> ta(insight::Analysis::lookup(analysisName_, parameters_, ""));
      ep = ta->createExecutionPathIfNonexistent();
    }
    else
    {
      throw insight::Exception("Execution path is needed but no current execution path is set!");
    }

    ui->localDir->setText( QString::fromStdString(ep.string()) );

  }
  else
  {
    ep = setPath.toStdString();
  }

  if (!boost::filesystem::exists(ep) && createIfNonexistent)
  {
    boost::filesystem::create_directories(ep);
  }

  return ep;
}