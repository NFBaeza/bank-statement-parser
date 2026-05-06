#include <QCoreApplication>
#include <QDebug>

#include "bank.h"
#include "bankFactory.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    auto bank = BankFactory::create(QStringLiteral("chile"),
                                    QStringLiteral("debit"));
    if (!bank) {
        qWarning() << "No concrete bank implementation registered.";
        return 1;
    }

    const QString filePath =
        QStringLiteral("files/Cartola-Emitida-Cuenta-1.pdf");

    qDebug().noquote() << "==== Raw PDF text dump ====";
    bank->printBankFile(filePath);

    qDebug().noquote() << "==== Parsed transactions ====";
    bank->readBankMovements(filePath);
    qDebug() << bank->nameBank;

    return 0;
}
