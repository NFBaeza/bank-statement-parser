#ifndef PDFPARSER_BICE_H
#define PDFPARSER_BICE_H

#include "bank.h"

namespace pdfparser {

class BICE : public Bank
{
public:
    explicit BICE(const QString &typeAccount);
    BICE(const QString &typeAccount, const QString &filePath);

protected:
    void readBankMovementsCredit(const QStringList &pagesText,
                                 QList<Transaction> &out) override;
    void readBankMovementsDebit (const QStringList &pagesText,
                                 QList<Transaction> &out) override;
};

} // namespace pdfparser

#endif // PDFPARSER_BICE_H
