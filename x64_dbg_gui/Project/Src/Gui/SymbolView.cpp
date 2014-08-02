#include "SymbolView.h"
#include "ui_SymbolView.h"
#include "Configuration.h"

SymbolView::SymbolView(QWidget *parent) : QWidget(parent), ui(new Ui::SymbolView)
{
    ui->setupUi(this);

    // Set main layout
    mMainLayout = new QVBoxLayout;
    mMainLayout->setContentsMargins(0, 0, 0, 0);
    mMainLayout->addWidget(ui->mainSplitter);
    setLayout(mMainLayout);

    // Create reference view
    mSearchListView = new SearchListView();
    mSearchListView->mSearchStartCol = 1;

    // Create module list
    mModuleList = new StdTable();
    int charwidth = mModuleList->getCharWidth();
    mModuleList->addColumnAt(charwidth*2*sizeof(int_t)+8, "Base", false);
    mModuleList->addColumnAt(0, "Module", true);

    // Setup symbol list
    mSearchListView->mList->addColumnAt(charwidth*2*sizeof(int_t)+8, "Address", true);
    mSearchListView->mList->addColumnAt(charwidth*80, "Symbol", true);
    mSearchListView->mList->addColumnAt(0, "Symbol (undecorated)", true);

    // Setup search list
    mSearchListView->mSearchList->addColumnAt(charwidth*2*sizeof(int_t)+8, "Address", true);
    mSearchListView->mSearchList->addColumnAt(charwidth*80, "Symbol", true);
    mSearchListView->mSearchList->addColumnAt(0, "Symbol (undecorated)", true);

    // Setup list splitter
    ui->listSplitter->addWidget(mModuleList);
    ui->listSplitter->addWidget(mSearchListView);
#ifdef _WIN64
    // mModuleList : mSymbolList = 40 : 100
    ui->listSplitter->setStretchFactor(0, 40);
    ui->listSplitter->setStretchFactor(1, 100);
#else
    // mModuleList : mSymbolList = 30 : 100
    ui->listSplitter->setStretchFactor(0, 30);
    ui->listSplitter->setStretchFactor(1, 100);
#endif //_WIN64

    // Setup log edit
    ui->symbolLogEdit->setFont(mModuleList->font());
    ui->symbolLogEdit->setStyleSheet("QTextEdit { background-color: rgb(255, 251, 240) }");
    ui->symbolLogEdit->setUndoRedoEnabled(false);
    ui->symbolLogEdit->setReadOnly(true);
    // Log : List = 2 : 9
    ui->mainSplitter->setStretchFactor(1, 9);
    ui->mainSplitter->setStretchFactor(0, 2);

    //setup context menu
    setupContextMenu();

    //Signals and slots
    connect(Bridge::getBridge(), SIGNAL(repaintTableView()), this, SLOT(updateStyle()));
    connect(Bridge::getBridge(), SIGNAL(addMsgToSymbolLog(QString)), this, SLOT(addMsgToSymbolLogSlot(QString)));
    connect(Bridge::getBridge(), SIGNAL(clearLog()), this, SLOT(clearSymbolLogSlot()));
    connect(Bridge::getBridge(), SIGNAL(clearSymbolLog()), this, SLOT(clearSymbolLogSlot()));
    connect(mModuleList, SIGNAL(selectionChangedSignal(int)), this, SLOT(moduleSelectionChanged(int)));
    connect(mModuleList, SIGNAL(contextMenuSignal(QPoint)), this, SLOT(moduleContextMenu(QPoint)));
    connect(mModuleList, SIGNAL(enterPressedSignal()), this, SLOT(moduleFollow()));
    connect(mModuleList, SIGNAL(doubleClickedSignal()), this, SLOT(moduleFollow()));
    connect(Bridge::getBridge(), SIGNAL(updateSymbolList(int,SYMBOLMODULEINFO*)), this, SLOT(updateSymbolList(int,SYMBOLMODULEINFO*)));
    connect(Bridge::getBridge(), SIGNAL(setSymbolProgress(int)), ui->symbolProgress, SLOT(setValue(int)));
    connect(Bridge::getBridge(), SIGNAL(symbolRefreshCurrent()), this, SLOT(symbolRefreshCurrent()));
    connect(mSearchListView, SIGNAL(listContextMenuSignal(QMenu*)), this, SLOT(symbolContextMenu(QMenu*)));
    connect(mSearchListView, SIGNAL(enterPressedSignal()), this, SLOT(symbolFollow()));
}

SymbolView::~SymbolView()
{
    delete ui;
}

void SymbolView::setupContextMenu()
{
    mFollowSymbolAction = new QAction("&Follow in Disassembler", this);
    mFollowSymbolAction->setShortcutContext(Qt::WidgetShortcut);
    mFollowSymbolAction->setShortcut(QKeySequence("enter"));
    connect(mFollowSymbolAction, SIGNAL(triggered()), this, SLOT(symbolFollow()));

    mFollowSymbolDumpAction = new QAction("Follow in &Dump", this);
    connect(mFollowSymbolDumpAction, SIGNAL(triggered()), this, SLOT(symbolFollowDump()));

    mFollowModuleAction = new QAction("&Follow in Disassembler", this);
    mFollowModuleAction->setShortcutContext(Qt::WidgetShortcut);
    mFollowModuleAction->setShortcut(QKeySequence("enter"));
    connect(mFollowModuleAction, SIGNAL(triggered()), this, SLOT(moduleFollow()));

    mDownloadSymbolsAction = new QAction("&Download Symbols", this);
    connect(mDownloadSymbolsAction, SIGNAL(triggered()), this, SLOT(moduleDownloadSymbols()));
}

void SymbolView::updateStyle()
{
    ui->symbolLogEdit->setStyleSheet(QString("QTextEdit { color: %1; background-color: %2 }").arg(ConfigColor("AbstractTableViewTextColor").name(), ConfigColor("AbstractTableViewBackgroundColor").name()));
}

void SymbolView::addMsgToSymbolLogSlot(QString msg)
{
    ui->symbolLogEdit->moveCursor(QTextCursor::End);
    ui->symbolLogEdit->insertPlainText(msg);
}

void SymbolView::clearSymbolLogSlot()
{
    ui->symbolLogEdit->clear();
}

void SymbolView::cbSymbolEnum(SYMBOLINFO* symbol, void* user)
{
    StdTable* symbolList=(StdTable*)user;
    int_t index=symbolList->getRowCount();
    symbolList->setRowCount(index+1);
    symbolList->setCellContent(index, 0, QString("%1").arg(symbol->addr, sizeof(int_t)*2, 16, QChar('0')).toUpper());
    if(symbol->decoratedSymbol)
    {
        symbolList->setCellContent(index, 1, symbol->decoratedSymbol);
        BridgeFree(symbol->decoratedSymbol);
    }
    if(symbol->undecoratedSymbol)
    {
        symbolList->setCellContent(index, 2, symbol->undecoratedSymbol);
        BridgeFree(symbol->undecoratedSymbol);
    }
}

void SymbolView::moduleSelectionChanged(int index)
{
    QString mod = mModuleList->getCellContent(index, 1);
    if(!mModuleBaseList.count(mod))
        return;
    mSearchListView->mList->setRowCount(0);
    DbgSymbolEnum(mModuleBaseList[mod], cbSymbolEnum, mSearchListView->mList);
    mSearchListView->mList->reloadData();
    mSearchListView->mList->setSingleSelection(0);
    mSearchListView->mList->setTableOffset(0);
    mSearchListView->mList->setFocus();
    mSearchListView->mSearchBox->setText("");
}

void SymbolView::updateSymbolList(int module_count, SYMBOLMODULEINFO* modules)
{
    mModuleList->setRowCount(module_count);
    if(!module_count)
    {
        mSearchListView->mList->setRowCount(0);
        mSearchListView->mList->setSingleSelection(0);
        mModuleList->setSingleSelection(0);
    }
    mModuleBaseList.clear();
    for(int i=0; i<module_count; i++)
    {
        mModuleBaseList.insert(modules[i].name, modules[i].base);
        mModuleList->setCellContent(i, 0, QString("%1").arg(modules[i].base, sizeof(int_t)*2, 16, QChar('0')).toUpper());
        mModuleList->setCellContent(i, 1, modules[i].name);
    }
    mModuleList->reloadData();
    if(modules)
        BridgeFree(modules);
}

void SymbolView::symbolContextMenu(QMenu* wMenu)
{
    if(!mSearchListView->mCurList->getRowCount())
        return;
    wMenu->addAction(mFollowSymbolAction);
    wMenu->addAction(mFollowSymbolDumpAction);
}

void SymbolView::symbolRefreshCurrent()
{
    mModuleList->setSingleSelection(mModuleList->getInitialSelection());
}

void SymbolView::symbolFollow()
{
    DbgCmdExecDirect(QString("disasm " + mSearchListView->mCurList->getCellContent(mSearchListView->mCurList->getInitialSelection(), 0)).toUtf8().constData());
    emit showCpu();
}

void SymbolView::symbolFollowDump()
{
    DbgCmdExecDirect(QString("dump " + mSearchListView->mCurList->getCellContent(mSearchListView->mCurList->getInitialSelection(), 0)).toUtf8().constData());
    emit showCpu();
}

void SymbolView::moduleContextMenu(const QPoint & pos)
{
    if(!DbgIsDebugging())
        return;
    QMenu* wMenu = new QMenu(this); //create context menu
    wMenu->addAction(mFollowModuleAction);
    wMenu->addAction(mDownloadSymbolsAction);
    QMenu wCopyMenu("&Copy", this);
    mModuleList->setupCopyMenu(&wCopyMenu);
    if(wCopyMenu.actions().length())
    {
        wMenu->addSeparator();
        wMenu->addMenu(&wCopyMenu);
    }
    wMenu->exec(mapToGlobal(pos)); //execute context menu
}

void SymbolView::moduleFollow()
{
    DbgCmdExecDirect(QString("disasm " + mModuleList->getCellContent(mModuleList->getInitialSelection(), 0) + "+1000").toUtf8().constData());
    emit showCpu();
}

void SymbolView::moduleDownloadSymbols()
{
    DbgCmdExec(QString("symdownload " + mModuleList->getCellContent(mModuleList->getInitialSelection(), 1)).toUtf8().constData());
}
