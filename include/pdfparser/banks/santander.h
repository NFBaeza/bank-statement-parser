#ifndef PDFPARSER_SANTANDER_H
#define PDFPARSER_SANTANDER_H

#include "bank.h"

namespace pdfparser {

class Santander : public Bank
{
public:
    explicit Santander(const QString &typeAccount);
    Santander(const QString &typeAccount, const QString &filePath);

protected:
    void readBankMovementsCredit(const QStringList &pagesText,
                                 QList<Transaction> &out) override;
    void readBankMovementsDebit (const QStringList &pagesText,
                                 QList<Transaction> &out) override;
};

} // namespace pdfparser

#endif // PDFPARSER_SANTANDER_H
