-- ============================================================================
-- SCHEMA V1 - INITIAL VERSION
-- Created: 2004-03-15
-- Author: J. Thompson (Legacy Systems Inc.)
-- Database: Sybase ASE 12.5
-- ============================================================================
-- $Header: /cvsroot/trading/db/schema_v1_initial.sql,v 1.45 2004/08/22 14:30:00 jthompson Exp $
-- $Revision: 1.45 $
-- ============================================================================

-- NOTE: This schema was designed for Sybase ASE 12.5
-- Migration to MS SQL Server 2000 completed 2005-01-10

IF EXISTS (SELECT * FROM sysobjects WHERE name = 'TRD_BLOTTER' AND type = 'U')
    DROP TABLE TRD_BLOTTER
GO

IF EXISTS (SELECT * FROM sysobjects WHERE name = 'CPTY_MSTR' AND type = 'U')
    DROP TABLE CPTY_MSTR
GO

IF EXISTS (SELECT * FROM sysobjects WHERE name = 'PRD_TYP_REF' AND type = 'U')
    DROP TABLE PRD_TYP_REF
GO

-- ============================================================================
-- COUNTERPARTY MASTER TABLE
-- ============================================================================
CREATE TABLE CPTY_MSTR (
    CPTY_ID          INT IDENTITY(1,1) NOT NULL,
    CPTY_CD          VARCHAR(12) NOT NULL,
    CPTY_NM          VARCHAR(64) NOT NULL,
    CPTY_STTUS_CD    CHAR(1) DEFAULT 'A',
    CRTD_BY          VARCHAR(32) NOT NULL,
    CRTD_TS          DATETIME DEFAULT GETDATE(),
    UPDTD_BY         VARCHAR(32) NULL,
    UPDTD_TS         DATETIME NULL,
    
    CONSTRAINT PK_CPTY_MSTR PRIMARY KEY (CPTY_ID),
    CONSTRAINT UK_CPTY_MSTR_CD UNIQUE (CPTY_CD)
)
GO

-- Legacy indexes from Sybase migration
CREATE INDEX IDX_CPTY_MSTR_CD ON CPTY_MSTR(CPTY_CD)
GO

CREATE INDEX IDX_CPTY_MSTR_STTUS ON CPTY_MSTR(CPTY_STTUS_CD)
GO

GRANT SELECT, INSERT, UPDATE, DELETE ON CPTY_MSTR TO trading_app_role
GO

-- ============================================================================
-- PRODUCT TYPE REFERENCE TABLE
-- ============================================================================
CREATE TABLE PRD_TYP_REF (
    PRD_TYP_ID       SMALLINT NOT NULL,
    PRD_TYP_CD       VARCHAR(8) NOT NULL,
    PRD_TYP_NM       VARCHAR(32) NOT NULL,
    PRD_TYP_STTUS    CHAR(1) DEFAULT 'A',
    CRTD_TS          DATETIME DEFAULT GETDATE(),
    UPDTD_TS         DATETIME NULL,
    
    CONSTRAINT PK_PRD_TYP_REF PRIMARY KEY (PRD_TYP_ID),
    CONSTRAINT UK_PRD_TYP_REF_CD UNIQUE (PRD_TYP_CD)
)
GO

-- Initial product types
INSERT INTO PRD_TYP_REF (PRD_TYP_ID, PRD_TYP_CD, PRD_TYP_NM) VALUES (1, 'IRS', 'Interest Rate Swap')
INSERT INTO PRD_TYP_REF (PRD_TYP_ID, PRD_TYP_CD, PRD_TYP_NM) VALUES (2, 'CCY', 'Cross Currency Swap')
INSERT INTO PRD_TYP_REF (PRD_TYP_ID, PRD_TYP_CD, PRD_TYP_NM) VALUES (3, 'FRA', 'Forward Rate Agreement')
INSERT INTO PRD_TYP_REF (PRD_TYP_ID, PRD_TYP_CD, PRD_TYP_NM) VALUES (4, 'CAP', 'Interest Rate Cap')
INSERT INTO PRD_TYP_REF (PRD_TYP_ID, PRD_TYP_CD, PRD_TYP_NM) VALUES (5, 'FLR', 'Interest Rate Floor')
INSERT INTO PRD_TYP_REF (PRD_TYP_ID, PRD_TYP_CD, PRD_TYP_NM) VALUES (6, 'SWPTN', 'Swaption')
GO

-- ============================================================================
-- TRADE BLOTTER TABLE (Main trading table)
-- ============================================================================
-- IMPORTANT: This table was designed for high-volume insert performance
-- Avoid adding columns without DBA review
-- ============================================================================
CREATE TABLE TRD_BLOTTER (
    TRD_ID           INT IDENTITY(1,1) NOT NULL,
    TRD_REF_NM       VARCHAR(24) NOT NULL,
    CPTY_ID          INT NOT NULL,
    PRD_TYP_ID       SMALLINT NOT NULL,
    TRD_STTUS_CD     CHAR(2) DEFAULT 'PE',
    TRD_DT           DATETIME NOT NULL,
    EFF_DT           DATETIME NOT NULL,
    MAT_DT           DATETIME NULL,
    CCY_CD           CHAR(3) DEFAULT 'USD',
    TRD_USR_ID       VARCHAR(16) NOT NULL,
    BOOK_NM          VARCHAR(16) NOT NULL,
    CRTD_TS          DATETIME DEFAULT GETDATE(),
    UPDTD_TS         DATETIME NULL,
    
    CONSTRAINT PK_TRD_BLOTTER PRIMARY KEY (TRD_ID),
    CONSTRAINT FK_TRD_BLOTTER_CPTY FOREIGN KEY (CPTY_ID) 
        REFERENCES CPTY_MSTR(CPTY_ID),
    CONSTRAINT FK_TRD_BLOTTER_PRD_TYP FOREIGN KEY (PRD_TYP_ID) 
        REFERENCES PRD_TYP_REF(PRD_TYP_ID)
)
GO

-- Indexes for common queries
CREATE INDEX IDX_TRD_BLOTTER_REF ON TRD_BLOTTER(TRD_REF_NM)
GO

CREATE INDEX IDX_TRD_BLOTTER_DT ON TRD_BLOTTER(TRD_DT)
GO

CREATE INDEX IDX_TRD_BLOTTER_CPTY ON TRD_BLOTTER(CPTY_ID)
GO

CREATE INDEX IDX_TRD_BLOTTER_STTUS ON TRD_BLOTTER(TRD_STTUS_CD)
GO

-- Clustered index on trade date for reporting
-- NOTE: Changed from TRD_ID to TRD_DT on 2004-06-15 per DBA recommendation
CREATE CLUSTERED INDEX IDX_TRD_BLOTTER_CLUS ON TRD_BLOTTER(TRD_DT)
GO

GRANT SELECT, INSERT, UPDATE, DELETE ON TRD_BLOTTER TO trading_app_role
GO

-- ============================================================================
-- TRADE STATUS REFERENCE
-- ============================================================================
CREATE TABLE TRD_STTUS_REF (
    TRD_STTUS_CD     CHAR(2) NOT NULL,
    TRD_STTUS_NM     VARCHAR(24) NOT NULL,
    TRD_STTUS_ORD    TINYINT NOT NULL,
    
    CONSTRAINT PK_TRD_STTUS_REF PRIMARY KEY (TRD_STTUS_CD)
)
GO

INSERT INTO TRD_STTUS_REF VALUES ('PE', 'Pending', 10)
INSERT INTO TRD_STTUS_REF VALUES ('VF', 'Verified', 20)
INSERT INTO TRD_STTUS_REF VALUES ('BK', 'Booked', 30)
INSERT INTO TRD_STTUS_REF VALUES ('CN', 'Cancelled', 99)
INSERT INTO TRD_STTUS_REF VALUES ('ER', 'Error', 98)
GO

-- ============================================================================
-- Legacy audit trigger (kept for backwards compatibility)
-- ============================================================================
-- NOTE: This trigger was causing deadlocks on Sybase, kept for reference
-- ============================================================================
-- CREATE TRIGGER TR_TRD_BLOTTER_AUDIT
-- ON TRD_BLOTTER AFTER INSERT, UPDATE, DELETE
-- AS
-- BEGIN
--     -- Audit logic removed 2004-11-20 due to performance issues
--     -- See change request CR-2004-1120
-- END
-- GO

-- $Log: schema_v1_initial.sql,v $
-- Revision 1.45  2004/08/22 14:30:00  jthompson
-- Added additional indexes per DBA request
--
-- Revision 1.44  2004/07/15 09:22:00  jthompson
-- Fixed typo in CPTY_STTUS_CD constraint
--
-- Revision 1.43  2004/06/15 16:45:00  jthompson
-- Changed clustered index from TRD_ID to TRD_DT
-- ============================================================================
