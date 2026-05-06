#ifndef PDFPARSER_CHILE_H
#define PDFPARSER_CHILE_H

#include "bank.h"

namespace pdfparser {

class Chile : public Bank
{
public:
    explicit Chile(const QString &typeAccount);
    Chile(const QString &typeAccount, const QString &filePath);

protected:
    void readBankMovementsCredit(const QStringList &pagesText,
                                 QList<Transaction> &out) override;
    void readBankMovementsDebit (const QStringList &pagesText,
                                 QList<Transaction> &out) override;
};

} // namespace pdfparser

#endif // PDFPARSER_CHILE_H
