#include "vendorledger.h"
#include "ui_vendorledger.h"

VendorLedger::VendorLedger(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::VendorLedger)
{
    ui->setupUi(this);
    timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(loadParams()) );
    timer->start(5);
}

VendorLedger::~VendorLedger()
{
    delete ui;
}



void VendorLedger::autocompleter(QString sql, QLineEdit *name_txt, QLineEdit *id_txt)
{
    sch.name_txt = name_txt;
    sch.id_txt = id_txt;
    QMap<int, QString> data = sch.data;
    QSqlQuery qry;
    qry.prepare(sql);
    if(qry.exec())
    {
        while(qry.next())
        {
            data[qry.value(0).toInt()] = qry.value(1).toString();
        }
    }
    QCompleter *completer = new QCompleter(this);
    QStandardItemModel *model = new QStandardItemModel(completer);
    QMapIterator<int, QString> it(data);
    while (it.hasNext())
    {
        it.next();
        int code = it.key();
        QString name = it.value();
        QStandardItem *item = new QStandardItem;
        item->setText(name);
        item->setData(code, Qt::UserRole);
        model->appendRow(item);
    }
    completer->setModel(model);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setCurrentRow(0);
    completer->setFilterMode(Qt::MatchContains);
    name_txt->setCompleter(completer);


    connect(completer, SIGNAL(highlighted(QModelIndex)),this,SLOT(onItemHighlighted(QModelIndex)),Qt::QueuedConnection);
    connect(name_txt,SIGNAL(editingFinished()),this,SLOT(editingFinished() ));
}

void VendorLedger::onItemHighlighted(const QModelIndex &index)
{
    QString code = index.data(Qt::UserRole).toString();
    QString sname = index.data(0).toString();
    sch.searchname = sname;
    sch.searchid = code;
    sch.id_txt->setText(code);
}
void VendorLedger::editingFinished()
{
    QString sname = sch.name_txt->text();
    QString sid = sch.id_txt->text();
    if(sname!=sch.searchname || sid != sch.searchid)
    {
        sch.name_txt->setText("");
        sch.id_txt->setText("");
    }
}

void VendorLedger::loadParams()
{
    timer->stop();
    ui->begindate->setDate( erp.reportBeginDate() );
    ui->enddate->setDate( QDate::currentDate() );
}

void VendorLedger::on_vendorname_textEdited(const QString &arg1)
{
    autocompleter(sch.vendors(arg1),ui->vendorname,ui->vendorid);
}

void VendorLedger::on_btn_next_clicked()
{
    if(ui->vendorid->text()=="")
    {
        QMessageBox::warning(this,"","Please select vendor");
        ui->vendorname->setFocus();
        return;
    }
    this->setCursor(Qt::WaitCursor);
    printer=new QPrinter();
    printer->setOutputFormat(QPrinter::PdfFormat);
    printer->setPaperSize(QPrinter::A4);
    printer->setPageMargins(QMarginsF(10,10,10,10));
    printDlg=new QPrintDialog(printer);
    pageDlg=new QPageSetupDialog(printer);

    pd = new QPrintPreviewDialog(printer);
    connect(pd,SIGNAL(paintRequested(QPrinter*)),this,SLOT(printPreview(QPrinter*)));
    pd->setWindowTitle("Vendor Ledger");
    pd->setWindowFlags(Qt::Window);
    pd->resize(900,600);
    pd->show();
}

void VendorLedger::printPreview(QPrinter *p)
{
    QString begindate = ui->begindate->date().toString("yyyy-MM-dd");
    QString enddate = ui->enddate->date().toString("yyyy-MM-dd");
    QString vendorid = ui->vendorid->text();

/******************************************report detail***********************************************/
    html = "";

    html += "<table width='100%' border='1' cellspacing='0' cellpadding='3'>"
            "<thead>"
            "<tr>"
            "<th colspan='6'>Vendor: "+erp.vendorname(vendorid)+"</th>"
            "</tr>";

    html +="<tr bgcolor='#f2f2f2'>"
               "<th width='10%'>Date</th>"
               "<th width='10%'>Voucher No</th>"
               "<th width='35%'>Description</th>"
               "<th width='15%'>Debit</th>"
               "<th width='15%'>Credit</th>"
               "<th width='15%'>Balance</th>"
           "</tr></thead>";
    QString p_acct = erp.getaccountAP("payable");
    QString query_open_dc = " select sum(debit) dr, sum(credit) cr  from tblgltransaction t, tblgltransvoucher v where "
                            " t.voucherno=v.voucherno and  t.companytype='vendor' and "
                            " t.compid='"+vendorid+"' and glaccountid='"+p_acct+"' and v.entrydate < '"+begindate+"' ";
    QSqlQuery info_open_dc(query_open_dc);
    float open_dr = info_open_dc.value("dr").toFloat();
    float open_cr = info_open_dc.value("cr").toFloat();
    float a_netopen = open_dr - open_cr;
    QString obal = erp.amountString(a_netopen)+" Dr";
    if(a_netopen<0)
    {
        obal = erp.amountString(a_netopen * -1)+" Cr";
    }

    html +="<tr>"
               "<th colspan='5' align='right'>Opening Balance</th>"
               "<th>"+ obal +"</th>"
           "</tr>";

    QString query_s = "SELECT v.voucherno, v.description, v.entrydate, v.companyid, t.debit dr, t.credit cr FROM "
                      " tblgltransvoucher v INNER JOIN tblgltransaction t ON (v.voucherno = t.voucherno) WHERE "
                      " t.compid = '"+vendorid+"' AND t.companytype = 'vendor' AND v.entrydate BETWEEN '"+begindate+"' "
                      " and  '"+enddate+"' AND t.glaccountid='"+p_acct+"' ORDER BY v.entrydate, v.voucherid asc ";


    QSqlQuery info_s(query_s);
    while(info_s.next())
    {
        float dr = info_s.value("dr").toFloat();
        float cr = info_s.value("cr").toFloat();
        a_netopen += (dr-cr);
        QString bal = erp.amountString(a_netopen)+" Dr";
        if(a_netopen<0)
        {
            bal = erp.amountString(a_netopen * -1)+" Cr";
        }
        html +="<tr>"
               "<td align='center'>"+info_s.value("entrydate").toDate().toString("dd/MM/yyyy")+"</td>"
               "<td align='center'>"+info_s.value("voucherno").toString()+"</td>"
               "<td>"+ info_s.value("description").toString() +"</td>"
               "<td align='right'>"+ erp.amountString(dr) +"</td>"
               "<td align='right'>"+ erp.amountString(cr) +"</td>"
               "<td align='right'>"+ bal +"</td>"
               "</tr>";
    }//end while

    QString cbal = erp.amountString(a_netopen)+" Dr";
    if(a_netopen<0)
    {
        cbal = erp.amountString(a_netopen * -1)+" Cr";
    }

    html +="<tr>"
               "<th colspan='5' align='right'>Closing Balance</th>"
               "<th>"+cbal+"</th>"
           "</tr>";

    html +="</table>";

/******************************************* Header****************************************************/

    QString header_html = "<table width='100%'>"
                          "<tr>"
                          "<td rowspan='2' width='20%' valign='bottom'>Print Date: "+QDate::currentDate().toString("dd/MM/yyyy")+"</td>"
                          "<td width='60%' align='center' style='font-size:22px;text-transform:uppercase;'><b>"+erp.companyname()+"</b></td>"
                          "<td rowspan='2' width='20%' align='right' valign='bottom'></td>"
                          "</tr>"
                          "<tr>"
                          "<th style='font-size:16px;'>Vendor Ledger</th>"
                          "</tr>"
                          "<tr><th colspan='3' align='center'>From Date: "+erp.DDMMYYDateFromSQL(begindate)+" To Date: "+erp.DDMMYYDateFromSQL(enddate)+"</th></tr>"
                          "</table>";


/******************************************* Settings ****************************************************/

    QRect printer_rect(p->pageRect());

    //Setting up the header and calculating the header size
        QTextDocument *document_header = new QTextDocument(this);
        document_header->setPageSize(printer_rect.size());
        //int pagenum = document_header->pageCount();
        document_header->setHtml(header_html);
        QSizeF header_size = document_header->size();

    //Setting up the footer and calculating the footer size
        QTextDocument *document_footer = new QTextDocument(this);
        document_footer->setPageSize(printer_rect.size());
        //document_footer->setHtml("");
        //QSizeF footer_size = document_footer->size();

    //Calculating the main document size for one page
        QSizeF center_size(printer_rect.width(), (p->pageRect().height() - header_size.toSize().height()  ));//- footer_size.toSize().height()

    //Setting up the center page
        QTextDocument *main_doc = new QTextDocument(this);
        main_doc->setHtml(html);
        main_doc->setPageSize(center_size);


    //Setting up the rectangles for each section.
        QRect pageNo = QRect(700, 40, 100, 50);
        QRect headerRect = QRect(QPoint(0,0), document_header->size().toSize());
        QRect footerRect = QRect(QPoint(0,0), document_footer->size().toSize());
        QRect contentRect = QRect(QPoint(0,0), main_doc->size().toSize());    // Main content rectangle.
        QRect currentRect = QRect(QPoint(0,0), center_size.toSize());        // Current main content rectangle.

        QPainter painter(p);
            pagenum=1;
            while (currentRect.intersects(contentRect))
            {//Loop if the current content rectangle intersects with the main content rectangle.
                //Resetting the painter matrix co ordinate system.
                painter.resetMatrix();
                //Applying negative translation of painter co-ordinate system by current main content rectangle top y coordinate.
                painter.translate(0, -currentRect.y());
                //Applying positive translation of painter co-ordinate system by header hight.
                painter.translate(0, headerRect.height());
                //Drawing the center content for current page.
                main_doc->drawContents(&painter, currentRect);
                //Resetting the painter matrix co ordinate system.
                painter.resetMatrix();
                //Drawing the header on the top of the page
                document_header->drawContents(&painter, headerRect);
                painter.drawText(pageNo,"Page. "+erp.intString(pagenum));
                //Applying positive translation of painter co-ordinate system to draw the footer
                painter.translate(0, headerRect.height());
                painter.translate(0, center_size.height());
                document_footer->drawContents(&painter, footerRect);

                //Translating the current rectangle to the area to be printed for the next page
                currentRect.translate(0, currentRect.height());
                //Inserting a new page if there is till area left to be printed
                if (currentRect.intersects(contentRect))
                {
                    p->newPage();
                    pagenum++;
                }
            }
this->setCursor(Qt::ArrowCursor);
}
