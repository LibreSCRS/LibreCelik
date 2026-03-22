// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

// This file exists solely to provide //% source-text comments for lupdate.
// It registers the English source strings for all qtTrId() IDs used in the
// project so that lupdate can populate <source> in the .ts files without
// clearing existing translations.
//
// The function is never called at runtime; it is compiled away by the
// optimiser. When adding a new qtTrId("lc-xxx") call anywhere in the project,
// add the corresponding QT_TRID_NOOP entry here so lupdate keeps working.

#include <QtCore/qglobal.h>
// qttranslation.h was introduced in Qt 6.5; use QCoreApplication as fallback.
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
#include <QtCore/qttranslation.h>
#else
#include <QtCore/QCoreApplication>
#endif

// QT_TRID_NOOP(id) expands to the bare string literal `id`, so every statement
// inside this function is an unused-value expression by design.
QT_WARNING_PUSH
QT_WARNING_DISABLE_CLANG("-Wunused-value")
QT_WARNING_DISABLE_GCC("-Wunused-value")

[[maybe_unused]] static void _translationsCatalog()
{
    // --- main window ---
    //% "<b>LibreCelik</b> :: Version %1 :: <a href=\"https://www.gnu.org/licenses/gpl-3.0.html\">GPL-3.0-or-later</a>
    //:: <a href=\"https://github.com/LibreSCRS/LibreCelik\">github.com/LibreSCRS/LibreCelik</a>"
    QT_TRID_NOOP("lc-main-about-librecelik");
    //% "Card in reader is not supported."
    QT_TRID_NOOP("lc-reader-unsupported-card");
    //% "Reading card..."
    QT_TRID_NOOP("lc-reading-card");
    //% "<b>LibreMiddleware</b> :: Version %1 :: <a
    // href=\"https://www.gnu.org/licenses/old-licenses/lgpl-2.1.html\">LGPL-2.1-or-later</a> :: <a
    // href=\"https://github.com/LibreSCRS/LibreMiddleware\">github.com/LibreSCRS/LibreMiddleware</a>"
    QT_TRID_NOOP("lc-main-about-libremiddleware");

    // --- generic ---
    //% "Unavailable"
    QT_TRID_NOOP("lc-doc-unavailable");

    // --- eID reader errors ---
    //% "Failed to connect to card."
    QT_TRID_NOOP("lc-eidreader-failed-connect");
    //% "PIN is blocked!"
    QT_TRID_NOOP("lc-eidreader-pin-blocked");
    //% "Incorrect PIN. Retries remaining: %1"
    QT_TRID_NOOP("lc-eidreader-pin-incorrect");
    //% "PIN change failed."
    QT_TRID_NOOP("lc-eidreader-pin-change-failed");
    //% "PIN change failed: %1"
    QT_TRID_NOOP("lc-eidreader-pin-change-exception");

    // --- eID print document field labels ---
    //% "ELECTRONIC ID CARD READER: DATA PRINTING"
    QT_TRID_NOOP("lc-eid-doc-title");
    //% "Foreigner id"
    QT_TRID_NOOP("lc-eid-doc-foreigner-id");
    //% "Printing date"
    QT_TRID_NOOP("lc-eid-doc-printing-date");
    //% "Foreigner Data"
    QT_TRID_NOOP("lc-eid-foreigner-data");
    //% "Citizen Data"
    QT_TRID_NOOP("lc-eid-citizen-data");
    //% "Surname"
    QT_TRID_NOOP("lc-eid-doc-surname");
    //% "Name"
    QT_TRID_NOOP("lc-eid-doc-name");
    //% "Parent name"
    QT_TRID_NOOP("lc-eid-doc-parent-name");
    //% "Nationality"
    QT_TRID_NOOP("lc-eid-doc-nationality");
    //% "Date of birth"
    QT_TRID_NOOP("lc-eid-doc-date-birth");
    //% "Place of birth"
    QT_TRID_NOOP("lc-eid-doc-place-birth");
    //% "Status of foreigner"
    QT_TRID_NOOP("lc-eid-doc-foreigner-status");
    //% "Date of address change"
    QT_TRID_NOOP("lc-eid-doc-address-change-date");
    //% "Gender"
    QT_TRID_NOOP("lc-eid-doc-gender");
    //% "Document data"
    QT_TRID_NOOP("lc-eid-doc-document-data");
    //% "Document issuer"
    QT_TRID_NOOP("lc-eid-doc-issuer");
    //% "Document number"
    QT_TRID_NOOP("lc-eid-doc-number");
    //% "Date of issuance"
    QT_TRID_NOOP("lc-eid-doc-issuance-date");
    //% "Valid to"
    QT_TRID_NOOP("lc-eid-doc-valid-to");

    // --- eID UI labels ---
    //% "Certificates"
    QT_TRID_NOOP("lc-eid-tree-certificates");
    //% "PIN"
    QT_TRID_NOOP("lc-eid-tree-pin");
    //% "Serbian eID"
    QT_TRID_NOOP("lc-eid-title-serbian");
    //% "Serbian eID"
    QT_TRID_NOOP("lc-eid-title");
    //% "Serbian eID (Foreigner)"
    QT_TRID_NOOP("lc-eid-title-foreigner");
    //% "JMBG"
    QT_TRID_NOOP("lc-eid-label-jmbg");
    //% "EBS"
    QT_TRID_NOOP("lc-eid-label-ebs");
    //% "Identity card"
    QT_TRID_NOOP("lc-eid-label-identity-card");
    //% "Identity card for foreigners"
    QT_TRID_NOOP("lc-eid-label-identity-foreigners");
    //% "Address"
    QT_TRID_NOOP("lc-eid-label-address");
    //% "Address"
    QT_TRID_NOOP("lc-eid-label-address-foreigner");
    //% "Given Name"
    QT_TRID_NOOP("lc-eid-label-given-name");
    //% "Surname"
    QT_TRID_NOOP("lc-eid-label-surname");
    //% "Date of Birth"
    QT_TRID_NOOP("lc-eid-label-date-of-birth");
    //% "Sex"
    QT_TRID_NOOP("lc-eid-label-sex");
    //% "Nationality"
    QT_TRID_NOOP("lc-eid-label-nationality");
    //% "Place of Birth"
    QT_TRID_NOOP("lc-eid-label-place-of-birth");
    //% "Parent Name"
    QT_TRID_NOOP("lc-eid-label-parent-name");
    //% "Date of Address Change"
    QT_TRID_NOOP("lc-eid-label-address-date");
    //% "Document Type"
    QT_TRID_NOOP("lc-eid-label-document-type");
    //% "Document Serial Number"
    QT_TRID_NOOP("lc-eid-label-document-serial-number");
    //% "Document"
    QT_TRID_NOOP("lc-eid-label-document");
    //% "Issuing Authority"
    QT_TRID_NOOP("lc-eid-label-issuing-authority");
    //% "Document Reg. No."
    QT_TRID_NOOP("lc-eid-label-doc-reg-no");
    //% "Issuing Date"
    QT_TRID_NOOP("lc-eid-label-issuing-date");
    //% "Expiry Date"
    QT_TRID_NOOP("lc-eid-label-expiry-date");
    //% "Card Certificate"
    QT_TRID_NOOP("lc-eid-label-card-verification");
    //% "Fixed Data"
    QT_TRID_NOOP("lc-eid-label-fixed-verification");
    //% "Variable Data"
    QT_TRID_NOOP("lc-eid-label-variable-verification");
    //% "can sign"
    QT_TRID_NOOP("lc-eid-cert-can-sign");
    //% "User PIN"
    QT_TRID_NOOP("lc-eid-pin-user");
    //% "BLOCKED"
    QT_TRID_NOOP("lc-eid-pin-blocked");
    //% "%1 tries remaining"
    QT_TRID_NOOP("lc-eid-pin-tries-remaining");
    //% "?"
    QT_TRID_NOOP("lc-eid-pin-unknown");
    //% "View Certificate"
    QT_TRID_NOOP("lc-eid-menu-view-cert");
    //% "Change PIN"
    QT_TRID_NOOP("lc-eid-menu-change-pin");
    //% "Initialize PIN"
    QT_TRID_NOOP("lc-eid-menu-initialize-pin");
    //% "Transport PIN (not initialized)"
    QT_TRID_NOOP("lc-eid-pin-transport");
    //% "Print Document"
    QT_TRID_NOOP("lc-eid-print-title");

    // --- Change PIN dialog ---
    //% "Initialize %1"
    QT_TRID_NOOP("lc-changepin-initialize-title");
    //% "Change %1"
    QT_TRID_NOOP("lc-changepin-change-title");
    //% "Transport PIN"
    QT_TRID_NOOP("lc-changepin-transport-placeholder");
    //% "PIN retries remaining: %1"
    QT_TRID_NOOP("lc-changepin-retries-remaining");
    //% "Current PIN"
    QT_TRID_NOOP("lc-changepin-current");
    //% "New PIN"
    QT_TRID_NOOP("lc-changepin-new");
    //% "Confirm new PIN"
    QT_TRID_NOOP("lc-changepin-confirm");
    //% "Changing PIN..."
    QT_TRID_NOOP("lc-changepin-changing");
    //% "PIN is blocked!"
    QT_TRID_NOOP("lc-changepin-blocked");
    //% "PIN changed successfully."
    QT_TRID_NOOP("lc-changepin-success");
    //% "Close"
    QT_TRID_NOOP("lc-changepin-close");
    //% "New PIN and confirmation do not match."
    QT_TRID_NOOP("lc-changepin-mismatch");
    //% "Show/Hide PIN"
    QT_TRID_NOOP("lc-changepin-show-hide");
    //% "PIN change failed."
    QT_TRID_NOOP("lc-changepin-failed");

    // --- Certificate viewer ---
    //% "Field"
    QT_TRID_NOOP("lc-cert-tree-field");
    //% "Value"
    QT_TRID_NOOP("lc-cert-tree-value");
    //% "Version"
    QT_TRID_NOOP("lc-cert-field-version");
    //% "Serial Number"
    QT_TRID_NOOP("lc-cert-field-serial-number");
    //% "Signature Algorithm"
    QT_TRID_NOOP("lc-cert-field-signature-algo");
    //% "Issuer"
    QT_TRID_NOOP("lc-cert-field-issuer");
    //% "Validity"
    QT_TRID_NOOP("lc-cert-field-validity");
    //% "Not Before"
    QT_TRID_NOOP("lc-cert-field-not-before");
    //% "Not After"
    QT_TRID_NOOP("lc-cert-field-not-after");
    //% "Subject"
    QT_TRID_NOOP("lc-cert-field-subject");
    //% "Public Key Info"
    QT_TRID_NOOP("lc-cert-field-public-key-info");
    //% "Algorithm"
    QT_TRID_NOOP("lc-cert-field-algorithm");
    //% "Key Size"
    QT_TRID_NOOP("lc-cert-field-key-size");
    //% "Extensions"
    QT_TRID_NOOP("lc-cert-field-extensions");
    //% " (Critical)"
    QT_TRID_NOOP("lc-cert-extension-critical");
    //% "Unknown"
    QT_TRID_NOOP("lc-cert-unknown");
    //% "Certificate Viewer"
    QT_TRID_NOOP("lc-cert-dialog-title");
    //% "No certificates available."
    QT_TRID_NOOP("lc-cert-no-available");

    // --- Certificate verification results ---
    //% "Valid"
    QT_TRID_NOOP("lc-cert-verify-valid");
    //% "Unspecified error"
    QT_TRID_NOOP("lc-cert-verify-unspecified-error");
    //% "Unable to get issuer certificate"
    QT_TRID_NOOP("lc-cert-verify-no-issuer");
    //% "Unable to get CRL"
    QT_TRID_NOOP("lc-cert-verify-no-crl");
    //% "Unable to decrypt certificate signature"
    QT_TRID_NOOP("lc-cert-verify-decrypt-fail");
    //% "Unable to decode issuer public key"
    QT_TRID_NOOP("lc-cert-verify-decode-fail");
    //% "Certificate not yet valid"
    QT_TRID_NOOP("lc-cert-verify-not-yet-valid");
    //% "Certificate has expired"
    QT_TRID_NOOP("lc-cert-verify-expired");
    //% "Certificate signature failure"
    QT_TRID_NOOP("lc-cert-verify-signature-fail");
    //% "Error in cert not before field"
    QT_TRID_NOOP("lc-cert-verify-not-before-error");
    //% "Error in cert not after field"
    QT_TRID_NOOP("lc-cert-verify-not-after-error");
    //% "Self-signed certificate"
    QT_TRID_NOOP("lc-cert-verify-self-signed");
    //% "Self-signed certificate in chain"
    QT_TRID_NOOP("lc-cert-verify-self-signed-chain");
    //% "Unable to get local issuer certificate"
    QT_TRID_NOOP("lc-cert-verify-no-local-issuer");
    //% "Unable to verify leaf signature"
    QT_TRID_NOOP("lc-cert-verify-leaf-fail");
    //% "Certificate revoked"
    QT_TRID_NOOP("lc-cert-verify-revoked");

    // --- Health insurance card ---
    //% "Health Insurance Card"
    QT_TRID_NOOP("lc-health-title");
    //% "Failed to connect to card."
    QT_TRID_NOOP("lc-healthreader-failed-connect");
    //% "Print Document"
    QT_TRID_NOOP("lc-health-print-title");
    //% "HEALTH INSURANCE CARD READER: DATA PRINTING"
    QT_TRID_NOOP("lc-health-doc-title");
    //% "Printing date"
    QT_TRID_NOOP("lc-health-doc-printing-date");
    //% "Personal Data"
    QT_TRID_NOOP("lc-health-section-personal");
    //% "Insurance Data"
    QT_TRID_NOOP("lc-health-section-insurance");
    //% "Address"
    QT_TRID_NOOP("lc-health-section-address");
    //% "Insurance Carrier Data"
    QT_TRID_NOOP("lc-health-section-carrier");
    //% "Employer / Taxpayer"
    QT_TRID_NOOP("lc-health-section-taxpayer");
    //% "Name"
    QT_TRID_NOOP("lc-health-label-given-name");
    //% "Name (Latin)"
    QT_TRID_NOOP("lc-health-label-given-name-lat");
    //% "Surname"
    QT_TRID_NOOP("lc-health-label-family-name");
    //% "Surname (Latin)"
    QT_TRID_NOOP("lc-health-label-family-name-lat");
    //% "Parent name"
    QT_TRID_NOOP("lc-health-label-parent-name");
    //% "Parent name (Latin)"
    QT_TRID_NOOP("lc-health-label-parent-name-lat");
    //% "Date of birth"
    QT_TRID_NOOP("lc-health-label-dob");
    //% "Gender"
    QT_TRID_NOOP("lc-health-label-gender");
    //% "Personal number (JMBG)"
    QT_TRID_NOOP("lc-health-label-jmbg");
    //% "Insurant number (LBO)"
    QT_TRID_NOOP("lc-health-label-lbo");
    //% "Insurer"
    QT_TRID_NOOP("lc-health-label-insurer");
    //% "Insurer ID"
    QT_TRID_NOOP("lc-health-label-insurer-id");
    //% "Card ID"
    QT_TRID_NOOP("lc-health-label-card-id");
    //% "Issue date"
    QT_TRID_NOOP("lc-health-label-issue-date");
    //% "Expiry date"
    QT_TRID_NOOP("lc-health-label-expiry");
    //% "Valid until"
    QT_TRID_NOOP("lc-health-label-valid-until");
    //% "Permanently valid"
    QT_TRID_NOOP("lc-health-label-permanently");
    //% "Street"
    QT_TRID_NOOP("lc-health-label-street");
    //% "Number"
    QT_TRID_NOOP("lc-health-label-number");
    //% "Apartment"
    QT_TRID_NOOP("lc-health-label-apartment");
    //% "Place"
    QT_TRID_NOOP("lc-health-label-place");
    //% "Municipality"
    QT_TRID_NOOP("lc-health-label-municipality");
    //% "Country"
    QT_TRID_NOOP("lc-health-label-country");
    //% "Carrier name"
    QT_TRID_NOOP("lc-health-label-carrier-name");
    //% "Carrier surname"
    QT_TRID_NOOP("lc-health-label-carrier-family-name");
    //% "Relationship"
    QT_TRID_NOOP("lc-health-label-carrier-relation");
    //% "Carrier ID"
    QT_TRID_NOOP("lc-health-label-carrier-id");
    //% "Carrier LBO"
    QT_TRID_NOOP("lc-health-label-carrier-lbo");
    //% "Employer name"
    QT_TRID_NOOP("lc-health-label-taxpayer-name");
    //% "Employer ID (PIB)"
    QT_TRID_NOOP("lc-health-label-taxpayer-id");
    //% "Employer residence"
    QT_TRID_NOOP("lc-health-label-taxpayer-res");
    //% "Activity code"
    QT_TRID_NOOP("lc-health-label-taxpayer-act");
    //% "Insurance basis"
    QT_TRID_NOOP("lc-health-label-insurance-basis");
    //% "Insurance description"
    QT_TRID_NOOP("lc-health-label-insurance-desc");
    //% "Insurance start"
    QT_TRID_NOOP("lc-health-label-insurance-start");
    //% "Yes"
    QT_TRID_NOOP("lc-health-val-yes");
    //% "No"
    QT_TRID_NOOP("lc-health-val-no");

    // --- PKS card ---
    //% "PKS Qualified Signature"
    QT_TRID_NOOP("lc-pks-title");
    //% "Qualified Electronic Certificate"
    QT_TRID_NOOP("lc-pks-header-title");
    //% "Certificates"
    QT_TRID_NOOP("lc-pks-cert-count");
    //% "Certificate"
    QT_TRID_NOOP("lc-pks-cert-label");
    //% "Card Type"
    QT_TRID_NOOP("lc-pks-card-type");

    // --- Token section (shared by eID, PKS, Health) ---
    //% "Token"
    QT_TRID_NOOP("lc-token-title");
    //% "View Certificates"
    QT_TRID_NOOP("lc-token-certs-button");
    //% "Change PIN"
    QT_TRID_NOOP("lc-token-change-pin");
    // Tree column headers
    //% "Object"
    QT_TRID_NOOP("lc-token-col-object");
    //% "Details"
    QT_TRID_NOOP("lc-token-col-details");
    // Key properties
    //% "Subject"
    QT_TRID_NOOP("lc-token-key-subject");
    //% "Algorithm"
    QT_TRID_NOOP("lc-token-key-algorithm");
    //% "Key usage"
    QT_TRID_NOOP("lc-token-key-usage");
    //% "Valid"
    QT_TRID_NOOP("lc-token-key-valid");
    //% "Private key"
    QT_TRID_NOOP("lc-token-key-private-key");
    // Key usage flags
    //% "Digital Signature"
    QT_TRID_NOOP("lc-token-ku-digital-signature");
    //% "Non-Repudiation"
    QT_TRID_NOOP("lc-token-ku-non-repudiation");
    //% "Key Encipherment"
    QT_TRID_NOOP("lc-token-ku-key-encipherment");
    //% "Data Encipherment"
    QT_TRID_NOOP("lc-token-ku-data-encipherment");
    //% "Key Agreement"
    QT_TRID_NOOP("lc-token-ku-key-agreement");

    // --- Shared UI ---
    //% "Print"
    QT_TRID_NOOP("lc-print-button");

    // --- Vehicle registration ---
    //% "Vehicle Registration"
    QT_TRID_NOOP("lc-vehicle-title");
    //% "Print Document"
    QT_TRID_NOOP("lc-vehicle-print-title");
    //% "Vehicle registration card reader"
    QT_TRID_NOOP("lc-vehicle-doc-title");
    //% "Printing date"
    QT_TRID_NOOP("lc-vehicle-doc-printing-date");
    //% "Registration number"
    QT_TRID_NOOP("lc-vehicle-doc-reg-number");
    //% "Date of issuance"
    QT_TRID_NOOP("lc-vehicle-doc-issuance-date");
    //% "Valid to"
    QT_TRID_NOOP("lc-vehicle-doc-valid-to");
    //% "State issuing"
    QT_TRID_NOOP("lc-vehicle-doc-state-issuing");
    //% "Competent authority"
    QT_TRID_NOOP("lc-vehicle-doc-competent-authority");
    //% "Authority issuing"
    QT_TRID_NOOP("lc-vehicle-doc-authority-issuing");
    //% "Unambiguous number"
    QT_TRID_NOOP("lc-vehicle-doc-unambiguous-no");
    //% "Serial number"
    QT_TRID_NOOP("lc-vehicle-doc-serial-no");
    //% "Owner data"
    QT_TRID_NOOP("lc-vehicle-doc-owner-data");
    //% "Surname / Business name"
    QT_TRID_NOOP("lc-vehicle-doc-owner-surname");
    //% "Name"
    QT_TRID_NOOP("lc-vehicle-doc-owner-name");
    //% "Address"
    QT_TRID_NOOP("lc-vehicle-doc-owner-address");
    //% "Personal number"
    QT_TRID_NOOP("lc-vehicle-doc-owner-personal-no");
    //% "Surname / Business name"
    QT_TRID_NOOP("lc-vehicle-doc-user-surname");
    //% "Name"
    QT_TRID_NOOP("lc-vehicle-doc-user-name");
    //% "Address"
    QT_TRID_NOOP("lc-vehicle-doc-user-address");
    //% "Personal number"
    QT_TRID_NOOP("lc-vehicle-doc-user-personal-no");
    //% "Vehicle data"
    QT_TRID_NOOP("lc-vehicle-doc-vehicle-data");
    //% "Date of first registration"
    QT_TRID_NOOP("lc-vehicle-doc-first-reg-date");
    //% "Year of production"
    QT_TRID_NOOP("lc-vehicle-doc-production-year");
    //% "Make"
    QT_TRID_NOOP("lc-vehicle-doc-make");
    //% "Type"
    QT_TRID_NOOP("lc-vehicle-doc-type");
    //% "Commercial description"
    QT_TRID_NOOP("lc-vehicle-doc-commercial-desc");
    //% "Type approval number"
    QT_TRID_NOOP("lc-vehicle-doc-type-approval-no");
    //% "Colour"
    QT_TRID_NOOP("lc-vehicle-doc-colour");
    //% "Number of axles"
    QT_TRID_NOOP("lc-vehicle-doc-axles");
    //% "VIN"
    QT_TRID_NOOP("lc-vehicle-doc-vin");
    //% "Capacity (cm3)"
    QT_TRID_NOOP("lc-vehicle-doc-capacity");
    //% "Engine number"
    QT_TRID_NOOP("lc-vehicle-doc-engine-number");
    //% "Mass (kg)"
    QT_TRID_NOOP("lc-vehicle-doc-mass");
    //% "Power (kW)"
    QT_TRID_NOOP("lc-vehicle-doc-power");
    //% "Load (kg)"
    QT_TRID_NOOP("lc-vehicle-doc-load");
    //% "Power/weight ratio"
    QT_TRID_NOOP("lc-vehicle-doc-power-weight");
    //% "Max laden mass (kg)"
    QT_TRID_NOOP("lc-vehicle-doc-max-laden-mass");
    //% "Category"
    QT_TRID_NOOP("lc-vehicle-doc-category");
    //% "Fuel type"
    QT_TRID_NOOP("lc-vehicle-doc-fuel-type");
    //% "Seats"
    QT_TRID_NOOP("lc-vehicle-doc-seats");
    //% "Standing places"
    QT_TRID_NOOP("lc-vehicle-doc-standing-places");

    // eMRTD (Electronic Passport)
    //% "Authentication Required"
    QT_TRID_NOOP("lc-emrtd-auth-required");
    //% "Authentication Failed"
    QT_TRID_NOOP("lc-emrtd-auth-failed");
    //% "Personal Data"
    QT_TRID_NOOP("lc-emrtd-personal-data");
    //% "Surname"
    QT_TRID_NOOP("lc-emrtd-surname");
    //% "Given Names"
    QT_TRID_NOOP("lc-emrtd-given-names");
    //% "Nationality"
    QT_TRID_NOOP("lc-emrtd-nationality");
    //% "Date of Birth"
    QT_TRID_NOOP("lc-emrtd-date-of-birth");
    //% "Sex"
    QT_TRID_NOOP("lc-emrtd-sex");
    //% "Document Data"
    QT_TRID_NOOP("lc-emrtd-document-data");
    //% "Document Number"
    QT_TRID_NOOP("lc-emrtd-doc-number");
    //% "Document Code"
    QT_TRID_NOOP("lc-emrtd-doc-code");
    //% "Issuing State"
    QT_TRID_NOOP("lc-emrtd-issuing-state");
    //% "Date of Expiry"
    QT_TRID_NOOP("lc-emrtd-date-of-expiry");
    //% "Personal Number"
    QT_TRID_NOOP("lc-emrtd-personal-number");
    //% "Additional Data"
    QT_TRID_NOOP("lc-emrtd-additional");
    //% "Issuing Information"
    QT_TRID_NOOP("lc-emrtd-issuing-info");
    //% "Electronic Passport Authentication"
    QT_TRID_NOOP("lc-emrtd-auth-dlg-title");
    //% "Foreigner ID Card"
    QT_TRID_NOOP("lc-emrtd-auth-dlg-subtitle");
    //% "CAN"
    QT_TRID_NOOP("lc-emrtd-auth-can-title");
    //% "6-digit number on card front"
    QT_TRID_NOOP("lc-emrtd-auth-can-desc");
    //% "MRZ"
    QT_TRID_NOOP("lc-emrtd-auth-mrz-title");
    //% "Data from document"
    QT_TRID_NOOP("lc-emrtd-auth-mrz-desc");
    //% "Document number"
    QT_TRID_NOOP("lc-emrtd-auth-mrz-docnum");
    //% "Date of birth"
    QT_TRID_NOOP("lc-emrtd-auth-mrz-dob");
    //% "Date of expiry"
    QT_TRID_NOOP("lc-emrtd-auth-mrz-expiry");
    //% "Authenticate"
    QT_TRID_NOOP("lc-emrtd-authenticate");
    //% "Authenticating..."
    QT_TRID_NOOP("lc-emrtd-authenticating");
}

QT_WARNING_POP
