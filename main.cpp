#include <QCoreApplication>
#include <QDebug>

#include "pdfparser/bank.h"
#include "pdfparser/bankFactory.h"

using pdfparser::BankFactory;

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    auto bank = BankFactory::create(QStringLiteral("estado"),
                                    QStringLiteral("debit"));
    if (!bank) {
        qWarning() << "No concrete bank implementation registered.";
        return 1;
    }

    const QString filePath =
        QStringLiteral("files/Ultimos_Movimientos_CuentaRUT.pdf");

    qDebug().noquote() << "==== Raw PDF text dump ====";
    bank->printBankFile(filePath);

    qDebug().noquote() << "==== Parsed transactions ====";
    bank->readBankMovements(filePath);
    bank->dumpTransactions();

    return 0;
}
