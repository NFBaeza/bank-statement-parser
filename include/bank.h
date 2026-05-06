#ifndef BANK_H
#define BANK_H

#include <QDateTime>
#include <QList>
#include <QRegularExpression>
#include <QString>
#include <QStringList>

#include "simpleClassifier.h"

class Bank
{
public:
    struct Transaction {
        QDateTime date;
        QString   category;
        QString   account;       
        double    amount {0.0};
        QString   description;
    };

    Bank(const QString &nameBank,
         const QString &typeAccount);

    Bank(const QString &nameBank,
         const QString &typeAccount,
         const QString &filePath);

    virtual ~Bank() = default;

    // Reads the PDF located at filePath, dispatching to the credit/debit
    // implementation defined by each derived bank class.
    void readBankMovements(const QString &filePath);

    // Read-only access to the parsed transactions from the most recent
    // readBankMovements() call.
    const QList<Transaction> &getTransactions() const { return transactions; }

    // Dumps the raw text content of each PDF page to qDebug() — useful while
    // reverse-engineering a new bank's statement layout.
    void printBankFile() const;
    void printBankFile(const QString &filePath) const;

    // Pretty-print parsed transactions to qDebug(). Test-driver convenience.
    void dumpTransactions() const;

    QString nameBank;
    QString typeAccount;
    QString filePath;

protected:
    // Each derived bank parses pre-extracted page text into transactions.
    virtual void readBankMovementsCredit(const QStringList &pagesText,
                                         QList<Transaction> &out) = 0;
    virtual void readBankMovementsDebit (const QStringList &pagesText,
                                         QList<Transaction> &out) = 0;

    // Helpers shared by every derived bank.
    QStringList extractPdfText(const QString &filePath) const;

    // Parse a Chilean-format amount: "$ 1.234.567" / "-1.234,50" / "5.000".
    // Dots are thousands separators; comma is the decimal point.
    static double parseClpAmount(const QString &raw);

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

    SimpleClassifier   classifier;
    QList<Transaction> transactions;
};

#endif // BANK_H
