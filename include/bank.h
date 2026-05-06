#ifndef BANK_H
#define BANK_H

#include <QString>
#include <QStringList>
#include <QList>
#include <QDateTime>
#include <QRegularExpression>

#include "simpleClassifier.h"

class Bank
{
public:
    Bank(const QString &nameBank,
         const QString &typeAccount);

    Bank(const QString &nameBank,
         const QString &typeAccount,
         const QString &filePath);

    virtual ~Bank() = default;

    // Reads the PDF located at filePath, dispatching to the credit/debit
    // implementation defined by each derived bank class.
    void readBankMovements(const QString &filePath);

    // Dumps the raw text content of each PDF page to qDebug() — useful while
    // reverse-engineering a new bank's statement layout.
    void printBankFile() const;
    void printBankFile(const QString &filePath) const;

    QString nameBank;
    QString typeAccount;
    QString filePath;

protected:
    struct Transaction {
        QDateTime date;
        QString   category;
        QString   account;
        double    amount {0.0};
        QString   description;
    };

    // Each derived bank parses pre-extracted page text into transactions.
    virtual void readBankMovementsCredit(const QStringList &pagesText,
                                         QList<Transaction> &out) = 0;
    virtual void readBankMovementsDebit (const QStringList &pagesText,
                                         QList<Transaction> &out) = 0;

    // Helpers shared by every derived bank.
    QStringList extractPdfText(const QString &filePath) const;
    QDateTime   castQDateTime(const QString &raw) const;

    // Spanish 3-letter month abbreviation -> 1..12, or 0 if unknown.
    // ("ene" -> 1, "abr" -> 4, "dic" -> 12).
    static int spanishMonth(const QString &name);

    // Strip embedded date/time/RUT/monto/operation-number noise from a
    // statement description. Call AFTER classifying — the noise carries
    // no semantic value but boilerplate prefixes ("Cargo por compra en")
    // are preserved.
    static QString cleanDescription(QString desc);

    // Split page text into logical rows, joining wraps onto their parent.
    // A new row is detected whenever a line matches `rowStartRx`; everything
    // else is appended to the row currently being assembled.
    static QStringList unwrapRows(const QString &pageText,
                                  const QRegularExpression &rowStartRx);

    SimpleClassifier classifier;
    QList<Transaction> transactions;
};

#endif // BANK_H
