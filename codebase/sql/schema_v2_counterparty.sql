-- ============================================================================
-- SCHEMA V2 - COUNTERPARTY ENHANCEMENTS
-- Created: 2007-05-22
-- Author: M. Rodriguez (DBA Team)
-- Database: MS SQL Server 2000 / 2005
-- ============================================================================
-- $Header: /cvsroot/trading/db/schema_v2_counterparty.sql,v 1.23 2007/09/14 11:00:00 mrodriguez Exp $
-- $Revision: 1.23 $
-- ============================================================================
-- 
-- MIGRATION NOTES:
-- This migration adds enhanced counterparty information for regulatory
-- compliance (Basel II requirements). Run during maintenance window.
--
-- IMPORTANT: Run dbcc checkdb before and after migration
-- ============================================================================

BEGIN TRANSACTION
GO

PRINT 'Starting Schema V2 Migration - Counterparty Enhancements'
PRINT GETDATE()
GO

-- ============================================================================
-- Add counterparty credit rating fields
-- ============================================================================
IF NOT EXISTS (SELECT * FROM syscolumns 
               WHERE id = OBJECT_ID('CPTY_MSTR') AND name = 'CRDRT_RATING_CD')
BEGIN
    ALTER TABLE CPTY_MSTR ADD CRDRT_RATING_CD VARCHAR(8) NULL
    PRINT 'Added CRDRT_RATING_CD column'
END
GO

IF NOT EXISTS (SELECT * FROM syscolumns 
               WHERE id = OBJECT_ID('CPTY_MSTR') AND name = 'CRDRT_AGENCY_NM')
BEGIN
    ALTER TABLE CPTY_MSTR ADD CRDRT_AGENCY_NM VARCHAR(32) NULL
    PRINT 'Added CRDRT_AGENCY_NM column'
END
GO

IF NOT EXISTS (SELECT * FROM syscolumns 
               WHERE id = OBJECT_ID('CPTY_MSTR') AND name = 'CPTY_CNTRY_CD')
BEGIN
    ALTER TABLE CPTY_MSTR ADD CPTY_CNTRY_CD CHAR(2) NULL
    PRINT 'Added CPTY_CNTRY_CD column'
END
GO

IF NOT EXISTS (SELECT * FROM syscolumns 
               WHERE id = OBJECT_ID('CPTY_MSTR') AND name = 'CPTY_LEI_CD')
BEGIN
    -- LEI not required until 2011, adding placeholder
    ALTER TABLE CPTY_MSTR ADD CPTY_LEI_CD VARCHAR(20) NULL
    PRINT 'Added CPTY_LEI_CD column (placeholder for future use)'
END
GO

-- ============================================================================
-- Create counterparty limit table
-- ============================================================================
IF EXISTS (SELECT * FROM sysobjects WHERE name = 'CPTY_LMT' AND type = 'U')
    DROP TABLE CPTY_LMT
GO

CREATE TABLE CPTY_LMT (
    CPTY_LMT_ID      INT IDENTITY(1,1) NOT NULL,
    CPTY_ID          INT NOT NULL,
    LMT_TYP_CD       VARCHAR(8) NOT NULL,
    LMT_AMT          DECIMAL(18,2) NOT NULL,
    LMT_CCY_CD       CHAR(3) DEFAULT 'USD',
    EFF_DT           DATETIME NOT NULL,
    EXP_DT           DATETIME NULL,
    CRTD_BY          VARCHAR(32) NOT NULL,
    CRTD_TS          DATETIME DEFAULT GETDATE(),
    UPDTD_BY         VARCHAR(32) NULL,
    UPDTD_TS         DATETIME NULL,
    
    CONSTRAINT PK_CPTY_LMT PRIMARY KEY (CPTY_LMT_ID),
    CONSTRAINT FK_CPTY_LMT_CPTY FOREIGN KEY (CPTY_ID) 
        REFERENCES CPTY_MSTR(CPTY_ID)
)
GO

CREATE INDEX IDX_CPTY_LMT_CPTY ON CPTY_LMT(CPTY_ID)
GO

CREATE INDEX IDX_CPTY_LMT_TYP ON CPTY_LMT(LMT_TYP_CD)
GO

-- ============================================================================
-- Limit type reference
-- ============================================================================
IF EXISTS (SELECT * FROM sysobjects WHERE name = 'LMT_TYP_REF' AND type = 'U')
    DROP TABLE LMT_TYP_REF
GO

CREATE TABLE LMT_TYP_REF (
    LMT_TYP_CD       VARCHAR(8) NOT NULL,
    LMT_TYP_NM       VARCHAR(32) NOT NULL,
    LMT_TYP_DESC     VARCHAR(128) NULL,
    
    CONSTRAINT PK_LMT_TYP_REF PRIMARY KEY (LMT_TYP_CD)
)
GO

INSERT INTO LMT_TYP_REF (LMT_TYP_CD, LMT_TYP_NM, LMT_TYP_DESC) 
VALUES ('CRD_EXPO', 'Credit Exposure', 'Maximum credit exposure limit')
INSERT INTO LMT_TYP_REF (LMT_TYP_CD, LMT_TYP_NM, LMT_TYP_DESC) 
VALUES ('DAILY_TURNOVER', 'Daily Turnover', 'Maximum daily trading turnover')
INSERT INTO LMT_TYP_REF (LMT_TYP_CD, LMT_TYP_NM, LMT_TYP_DESC) 
VALUES ('SETTLE_RISK', 'Settlement Risk', 'Maximum settlement risk exposure')
GO

-- ============================================================================
-- Add internal counterparty flag
-- ============================================================================
IF NOT EXISTS (SELECT * FROM syscolumns 
               WHERE id = OBJECT_ID('CPTY_MSTR') AND name = 'IS_INTRNL_FLG')
BEGIN
    ALTER TABLE CPTY_MSTR ADD IS_INTRNL_FLG CHAR(1) DEFAULT 'N'
    PRINT 'Added IS_INTRNL_FLG column'
END
GO

-- ============================================================================
-- Add counterparty relationship table (for netting agreements)
-- ============================================================================
IF EXISTS (SELECT * FROM sysobjects WHERE name = 'CPTY_RLTN' AND type = 'U')
    DROP TABLE CPTY_RLTN
GO

CREATE TABLE CPTY_RLTN (
    CPTY_RLTN_ID     INT IDENTITY(1,1) NOT NULL,
    CPTY_ID_1        INT NOT NULL,
    CPTY_ID_2        INT NOT NULL,
    RLTN_TYP_CD      VARCHAR(8) NOT NULL,
    NTTNG_AGRMT_ID   VARCHAR(32) NULL,
    EFF_DT           DATETIME NOT NULL,
    EXP_DT           DATETIME NULL,
    CRTD_TS          DATETIME DEFAULT GETDATE(),
    
    CONSTRAINT PK_CPTY_RLTN PRIMARY KEY (CPTY_RLTN_ID),
    CONSTRAINT FK_CPTY_RLTN_CPTY1 FOREIGN KEY (CPTY_ID_1) 
        REFERENCES CPTY_MSTR(CPTY_ID),
    CONSTRAINT FK_CPTY_RLTN_CPTY2 FOREIGN KEY (CPTY_ID_2) 
        REFERENCES CPTY_MSTR(CPTY_ID),
    CONSTRAINT CK_CPTY_RLTN_DIFF CHECK (CPTY_ID_1 <> CPTY_ID_2)
)
GO

CREATE INDEX IDX_CPTY_RLTN_CPTY1 ON CPTY_RLTN(CPTY_ID_1)
GO

CREATE INDEX IDX_CPTY_RLTN_CPTY2 ON CPTY_RLTN(CPTY_ID_2)
GO

-- ============================================================================
-- Migrate existing counterparty data with default values
-- ============================================================================
-- NOTE: This was a manual process in production, script for reference only
-- ============================================================================
-- UPDATE CPTY_MSTR 
-- SET CRDRT_RATING_CD = 'NR',
--     CPTY_CNTRY_CD = 'US'
-- WHERE CRDRT_RATING_CD IS NULL
-- GO

-- ============================================================================
-- Legacy fields from old system migration (DEPRECATED - DO NOT USE)
-- ============================================================================
-- These fields were migrated from the old 1998 trading system
-- They are kept for backwards compatibility only
-- ============================================================================
-- ALTER TABLE CPTY_MSTR ADD LEGACY_PTY_CD VARCHAR(16) NULL
-- ALTER TABLE CPTY_MSTR ADD LEGACY_SYS_ID VARCHAR(8) NULL
-- GO

PRINT 'Schema V2 Migration Complete'
PRINT GETDATE()
GO

COMMIT TRANSACTION
GO

-- $Log: schema_v2_counterparty.sql,v $
-- Revision 1.23  2007/09/14 11:00:00  mrodriguez
-- Added netting agreement fields
--
-- Revision 1.22  2007/08/01 14:22:00  mrodriguez
-- Fixed index naming convention
--
-- Revision 1.21  2007/06/15 09:00:00  mrodriguez
-- Initial counterparty enhancement migration
-- ============================================================================
