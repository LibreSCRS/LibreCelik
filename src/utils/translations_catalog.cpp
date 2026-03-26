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
    //% "Status of Foreigner"
    QT_TRID_NOOP("lc-eid-label-status-of-foreigner");

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

    // --- Error messages ---
    //% "No card connection available"
    QT_TRID_NOOP("lc-error-no-connection");
    //% "No plugin could read this card."
    QT_TRID_NOOP("lc-error-no-plugin");
    //% "Authentication failed"
    QT_TRID_NOOP("lc-error-auth-failed");

    // --- eMRTD ---
    //% "Insert the MRZ data from the bottom of the document"
    QT_TRID_NOOP("lc-emrtd-insert-mrz-hint");
    //% "Smart Card"
    QT_TRID_NOOP("lc-pkcs15-smart-card");

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
    //% "Family member"
    QT_TRID_NOOP("lc-health-label-family-member");
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
    //% "Print"
    QT_TRID_NOOP("lc-print-tooltip");

    // --- Vehicle registration ---
    //% "Vehicle Registration"
    QT_TRID_NOOP("lc-vehicle-title");
    //% "Print Document"
    QT_TRID_NOOP("lc-vehicle-print-title");

    // --- EU VRC (Vehicle Registration Certificate) ---
    //% "Vehicle Registration"
    QT_TRID_NOOP("lc-euvrc-title");
    //% "Print Document"
    QT_TRID_NOOP("lc-euvrc-print-title");
    //% "Registration"
    QT_TRID_NOOP("lc-euvrc-section-registration");
    //% "Vehicle"
    QT_TRID_NOOP("lc-euvrc-section-vehicle");
    //% "Engine & Technical"
    QT_TRID_NOOP("lc-euvrc-section-engine");
    //% "Holder"
    QT_TRID_NOOP("lc-euvrc-section-holder");
    //% "User"
    QT_TRID_NOOP("lc-euvrc-section-user");
    //% "National Extensions"
    QT_TRID_NOOP("lc-euvrc-section-national");
    //% "Owner"
    QT_TRID_NOOP("lc-euvrc-section-owner");
    //% "Surname or business name (C.2)"
    QT_TRID_NOOP("lc-euvrc-owner-name");

    // --- EU VRC header fields ---
    //% "Registration (A)"
    QT_TRID_NOOP("lc-euvrc-hdr-registration");
    //% "Make (D.1)"
    QT_TRID_NOOP("lc-euvrc-hdr-make");
    //% "Member State"
    QT_TRID_NOOP("lc-euvrc-hdr-member-state");
    //% "Valid to (H)"
    QT_TRID_NOOP("lc-euvrc-hdr-valid-to");

    // --- EU VRC registration field labels ---
    //% "Registration number (A)"
    QT_TRID_NOOP("lc-euvrc-reg-number");
    //% "Date of first registration (B)"
    QT_TRID_NOOP("lc-euvrc-reg-first-date");
    //% "Registration date (I)"
    QT_TRID_NOOP("lc-euvrc-reg-date");
    //% "Expiry date (H)"
    QT_TRID_NOOP("lc-euvrc-reg-expiry");
    //% "Document number"
    QT_TRID_NOOP("lc-euvrc-reg-doc-number");
    //% "Issuing authority"
    QT_TRID_NOOP("lc-euvrc-reg-issuing-auth");
    //% "Competent authority"
    QT_TRID_NOOP("lc-euvrc-reg-competent-auth");
    //% "Member state"
    QT_TRID_NOOP("lc-euvrc-reg-member-state");
    //% "Type approval number (K)"
    QT_TRID_NOOP("lc-euvrc-reg-type-approval");
    //% "Ownership status (C.4)"
    QT_TRID_NOOP("lc-euvrc-reg-ownership-status");
    //% "Previous document"
    QT_TRID_NOOP("lc-euvrc-reg-previous-document");

    // --- EU VRC vehicle field labels ---
    //% "Make (D.1)"
    QT_TRID_NOOP("lc-euvrc-veh-make");
    //% "Type (D.2)"
    QT_TRID_NOOP("lc-euvrc-veh-type");
    //% "Commercial description (D.3)"
    QT_TRID_NOOP("lc-euvrc-veh-commercial-desc");
    //% "VIN (E)"
    QT_TRID_NOOP("lc-euvrc-veh-vin");
    //% "Category (J)"
    QT_TRID_NOOP("lc-euvrc-veh-category");
    //% "Colour (R)"
    QT_TRID_NOOP("lc-euvrc-veh-colour");
    //% "Maximum speed (T)"
    QT_TRID_NOOP("lc-euvrc-veh-max-speed");

    // --- EU VRC engine & technical field labels ---
    //% "Engine capacity (P.1)"
    QT_TRID_NOOP("lc-euvrc-eng-capacity");
    //% "Maximum net power (P.2)"
    QT_TRID_NOOP("lc-euvrc-eng-max-power");
    //% "Type of fuel (P.3)"
    QT_TRID_NOOP("lc-euvrc-eng-fuel-type");
    //% "Engine ID number (P.5)"
    QT_TRID_NOOP("lc-euvrc-eng-id-number");
    //% "Vehicle mass (G)"
    QT_TRID_NOOP("lc-euvrc-eng-vehicle-mass");
    //% "Max permissible laden mass (F.1)"
    QT_TRID_NOOP("lc-euvrc-eng-max-laden-mass");
    //% "Power/weight ratio (Q)"
    QT_TRID_NOOP("lc-euvrc-eng-power-weight");
    //% "Number of seats (S.1)"
    QT_TRID_NOOP("lc-euvrc-eng-seats");
    //% "Standing places (S.2)"
    QT_TRID_NOOP("lc-euvrc-eng-standing");
    //% "Number of axles (L)"
    QT_TRID_NOOP("lc-euvrc-eng-axles");
    //% "Wheelbase (M)"
    QT_TRID_NOOP("lc-euvrc-eng-wheelbase");
    //% "Max laden mass in service (F.2)"
    QT_TRID_NOOP("lc-euvrc-eng-max-laden-mass-service");
    //% "Max laden mass whole vehicle (F.3)"
    QT_TRID_NOOP("lc-euvrc-eng-max-laden-mass-whole");
    //% "Max braked trailer mass (O.1)"
    QT_TRID_NOOP("lc-euvrc-eng-braked-trailer");
    //% "Max unbraked trailer mass (O.2)"
    QT_TRID_NOOP("lc-euvrc-eng-unbraked-trailer");
    //% "Rated engine speed (P.4)"
    QT_TRID_NOOP("lc-euvrc-eng-rated-engine-speed");
    //% "Stationary sound level (U.1)"
    QT_TRID_NOOP("lc-euvrc-eng-stationary-sound");
    //% "Engine speed for sound ref (U.2)"
    QT_TRID_NOOP("lc-euvrc-eng-engine-speed-ref");
    //% "Drive-by sound level (U.3)"
    QT_TRID_NOOP("lc-euvrc-eng-drive-by-sound");
    //% "Fuel consumption (V.7)"
    QT_TRID_NOOP("lc-euvrc-eng-fuel-consumption");
    //% "CO2 emissions (V.7)"
    QT_TRID_NOOP("lc-euvrc-eng-co2");
    //% "Environmental category (V.9)"
    QT_TRID_NOOP("lc-euvrc-eng-env-category");
    //% "Fuel tank capacity (W)"
    QT_TRID_NOOP("lc-euvrc-eng-fuel-tank");

    // --- EU VRC holder field labels ---
    //% "Surname or business name (C.1.1)"
    QT_TRID_NOOP("lc-euvrc-holder-name");
    //% "Other names (C.1.2)"
    QT_TRID_NOOP("lc-euvrc-holder-other-names");
    //% "Address (C.1.3)"
    QT_TRID_NOOP("lc-euvrc-holder-address");

    // --- EU VRC user field labels ---
    //% "Surname or business name (C.3)"
    QT_TRID_NOOP("lc-euvrc-user-name");
    //% "Other names (C.3)"
    QT_TRID_NOOP("lc-euvrc-user-other-names");
    //% "Address (C.3)"
    QT_TRID_NOOP("lc-euvrc-user-address");

    // --- EU VRC national extension labels (Serbian) ---
    //% "Owner Personal Number"
    QT_TRID_NOOP("lc-euvrc-nat-owners-personal-no");
    //% "User Personal Number"
    QT_TRID_NOOP("lc-euvrc-nat-users-personal-no");
    //% "Vehicle Load"
    QT_TRID_NOOP("lc-euvrc-nat-vehicle-load");
    //% "Year of Production"
    QT_TRID_NOOP("lc-euvrc-nat-year-of-production");
    //% "Serial Number"
    QT_TRID_NOOP("lc-euvrc-nat-serial-number");

    // --- EU VRC print document labels ---
    //% "EU Vehicle Registration Certificate"
    QT_TRID_NOOP("lc-euvrc-doc-title");
    //% "Printing date"
    QT_TRID_NOOP("lc-euvrc-doc-printing-date");
    //% "Registration number (A)"
    QT_TRID_NOOP("lc-euvrc-doc-reg-number");
    //% "Date of first registration (B)"
    QT_TRID_NOOP("lc-euvrc-doc-first-reg-date");
    //% "Registration date (I)"
    QT_TRID_NOOP("lc-euvrc-doc-reg-date");
    //% "Expiry date (H)"
    QT_TRID_NOOP("lc-euvrc-doc-expiry-date");
    //% "Member state"
    QT_TRID_NOOP("lc-euvrc-doc-member-state");
    //% "Document number"
    QT_TRID_NOOP("lc-euvrc-doc-document-number");
    //% "Competent authority"
    QT_TRID_NOOP("lc-euvrc-doc-competent-authority");
    //% "Issuing authority"
    QT_TRID_NOOP("lc-euvrc-doc-issuing-authority");
    //% "Type approval number (K)"
    QT_TRID_NOOP("lc-euvrc-doc-type-approval-no");
    //% "Vehicle Data"
    QT_TRID_NOOP("lc-euvrc-doc-vehicle-data");
    //% "Make (D.1)"
    QT_TRID_NOOP("lc-euvrc-doc-make");
    //% "Type (D.2)"
    QT_TRID_NOOP("lc-euvrc-doc-type");
    //% "Commercial description (D.3)"
    QT_TRID_NOOP("lc-euvrc-doc-commercial-desc");
    //% "VIN (E)"
    QT_TRID_NOOP("lc-euvrc-doc-vin");
    //% "Category (J)"
    QT_TRID_NOOP("lc-euvrc-doc-category");
    //% "Colour (R)"
    QT_TRID_NOOP("lc-euvrc-doc-colour");
    //% "Maximum speed (T)"
    QT_TRID_NOOP("lc-euvrc-doc-max-speed");
    //% "Engine & Technical"
    QT_TRID_NOOP("lc-euvrc-doc-engine-technical");
    //% "Engine capacity (P.1)"
    QT_TRID_NOOP("lc-euvrc-doc-capacity");
    //% "Maximum net power (P.2)"
    QT_TRID_NOOP("lc-euvrc-doc-power");
    //% "Type of fuel (P.3)"
    QT_TRID_NOOP("lc-euvrc-doc-fuel-type");
    //% "Engine ID number (P.5)"
    QT_TRID_NOOP("lc-euvrc-doc-engine-number");
    //% "Vehicle mass (G)"
    QT_TRID_NOOP("lc-euvrc-doc-mass");
    //% "Max permissible laden mass (F.1)"
    QT_TRID_NOOP("lc-euvrc-doc-max-laden-mass");
    //% "Power/weight ratio (Q)"
    QT_TRID_NOOP("lc-euvrc-doc-power-weight");
    //% "Number of axles (L)"
    QT_TRID_NOOP("lc-euvrc-doc-axles");
    //% "Number of seats (S.1)"
    QT_TRID_NOOP("lc-euvrc-doc-seats");
    //% "Standing places (S.2)"
    QT_TRID_NOOP("lc-euvrc-doc-standing-places");
    //% "Wheelbase (M)"
    QT_TRID_NOOP("lc-euvrc-doc-wheelbase");
    //% "Holder Data"
    QT_TRID_NOOP("lc-euvrc-doc-holder-data");
    //% "Surname or business name (C.1.1)"
    QT_TRID_NOOP("lc-euvrc-doc-holder-name");
    //% "Other names (C.1.2)"
    QT_TRID_NOOP("lc-euvrc-doc-holder-other-names");
    //% "Address (C.1.3)"
    QT_TRID_NOOP("lc-euvrc-doc-holder-address");
    //% "User Data"
    QT_TRID_NOOP("lc-euvrc-doc-user-data");
    //% "Surname or business name (C.3)"
    QT_TRID_NOOP("lc-euvrc-doc-user-name");
    //% "Other names (C.3)"
    QT_TRID_NOOP("lc-euvrc-doc-user-other-names");
    //% "Address (C.3)"
    QT_TRID_NOOP("lc-euvrc-doc-user-address");
    //% "Ownership status (C.4)"
    QT_TRID_NOOP("lc-euvrc-doc-ownership-status");
    //% "Previous document"
    QT_TRID_NOOP("lc-euvrc-doc-previous-document");
    //% "Owner Data"
    QT_TRID_NOOP("lc-euvrc-doc-owner-data");
    //% "Surname or business name (C.2)"
    QT_TRID_NOOP("lc-euvrc-doc-owner-name");
    //% "National Extensions"
    QT_TRID_NOOP("lc-euvrc-doc-national-data");
    //% "Max laden mass in service (F.2)"
    QT_TRID_NOOP("lc-euvrc-doc-max-laden-mass-service");
    //% "Max laden mass whole vehicle (F.3)"
    QT_TRID_NOOP("lc-euvrc-doc-max-laden-mass-whole");
    //% "Max braked trailer mass (O.1)"
    QT_TRID_NOOP("lc-euvrc-doc-braked-trailer");
    //% "Max unbraked trailer mass (O.2)"
    QT_TRID_NOOP("lc-euvrc-doc-unbraked-trailer");
    //% "Rated engine speed (P.4)"
    QT_TRID_NOOP("lc-euvrc-doc-rated-engine-speed");
    //% "Stationary sound level (U.1)"
    QT_TRID_NOOP("lc-euvrc-doc-stationary-sound");
    //% "Engine speed for sound ref (U.2)"
    QT_TRID_NOOP("lc-euvrc-doc-engine-speed-ref");
    //% "Drive-by sound level (U.3)"
    QT_TRID_NOOP("lc-euvrc-doc-drive-by-sound");
    //% "Fuel consumption (V.7)"
    QT_TRID_NOOP("lc-euvrc-doc-fuel-consumption");
    //% "CO2 emissions (V.7)"
    QT_TRID_NOOP("lc-euvrc-doc-co2");
    //% "Environmental category (V.9)"
    QT_TRID_NOOP("lc-euvrc-doc-env-category");
    //% "Fuel tank capacity (W)"
    QT_TRID_NOOP("lc-euvrc-doc-fuel-tank");

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
    // clang-format off
    //% "Authentication failed — travel document data could not be read. Check CAN or MRZ and reinsert the card to try again."
    // clang-format on
    QT_TRID_NOOP("lc-emrtd-no-data-message");
    //% "Personal Data"
    QT_TRID_NOOP("lc-personal-data-title");
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
    //% "CAN"
    QT_TRID_NOOP("lc-emrtd-auth-can-tab");
    //% "MRZ"
    QT_TRID_NOOP("lc-emrtd-auth-mrz-tab");

    //% "Travel Document"
    QT_TRID_NOOP("lc-emrtd-travel-document");
    //% "Signature / Mark"
    QT_TRID_NOOP("lc-emrtd-signature");
    //% "Issuing Authority"
    QT_TRID_NOOP("lc-emrtd-issuing-authority");
    //% "Date of Issue"
    QT_TRID_NOOP("lc-emrtd-date-of-issue");
    //% "Endorsements"
    QT_TRID_NOOP("lc-emrtd-endorsements");
    //% "Tax/Exit Requirements"
    QT_TRID_NOOP("lc-emrtd-tax-exit");
    //% "Full Name"
    QT_TRID_NOOP("lc-emrtd-full-name");
    //% "Other Names"
    QT_TRID_NOOP("lc-emrtd-other-names");
    //% "Place of Birth"
    QT_TRID_NOOP("lc-emrtd-place-of-birth");
    //% "Address"
    QT_TRID_NOOP("lc-emrtd-address");
    //% "Telephone"
    QT_TRID_NOOP("lc-emrtd-telephone");
    //% "Profession"
    QT_TRID_NOOP("lc-emrtd-profession");
    //% "Title"
    QT_TRID_NOOP("lc-emrtd-title");
    //% "Custody Information"
    QT_TRID_NOOP("lc-emrtd-custody-info");

    // eMRTD print-only translations
    //% "TRAVEL DOCUMENT: DATA PRINTING"
    QT_TRID_NOOP("lc-emrtd-doc-title");
    //% "Printing date"
    QT_TRID_NOOP("lc-emrtd-doc-printing-date");
    //% "Sex"
    QT_TRID_NOOP("lc-emrtd-doc-sex");

    // --- eMRTD security status ---
    //% "Security Status"
    QT_TRID_NOOP("lc-emrtd-security-status");
    //% "Data Integrity"
    QT_TRID_NOOP("lc-emrtd-security-integrity");
    //% "Data Authenticity"
    QT_TRID_NOOP("lc-emrtd-security-authenticity");
    //% "Chip Genuineness"
    QT_TRID_NOOP("lc-emrtd-security-genuineness");
    //% "Passed"
    QT_TRID_NOOP("lc-emrtd-security-passed");
    //% "Failed"
    QT_TRID_NOOP("lc-emrtd-security-failed");
    //% "Not Supported"
    QT_TRID_NOOP("lc-emrtd-security-not-supported");
    //% "Skipped"
    QT_TRID_NOOP("lc-emrtd-security-skipped");
    //% "Not Performed"
    QT_TRID_NOOP("lc-emrtd-security-not-performed");
    //% "Detailed Checks"
    QT_TRID_NOOP("lc-emrtd-security-details");

    // --- eMRTD new DG sections ---
    //% "Portrait"
    QT_TRID_NOOP("lc-emrtd-portrait");
    //% "Contacts"
    QT_TRID_NOOP("lc-emrtd-contacts");
    //% "Contact Name"
    QT_TRID_NOOP("lc-emrtd-contact-name");
    //% "Fingerprint Biometrics"
    QT_TRID_NOOP("lc-emrtd-biometric-fingerprint");
    //% "Iris Biometrics"
    QT_TRID_NOOP("lc-emrtd-biometric-iris");
    //% "Access restricted (EAC required)"
    QT_TRID_NOOP("lc-emrtd-biometric-eac-required");
    //% "National Data"
    QT_TRID_NOOP("lc-emrtd-national-data");
    //% "Tag"
    QT_TRID_NOOP("lc-emrtd-national-tag");
    //% "Value"
    QT_TRID_NOOP("lc-emrtd-national-value");
}

QT_WARNING_POP
