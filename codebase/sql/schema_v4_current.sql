-- ============================================================================
-- SCHEMA V4 - CURRENT VERSION
-- Created: 2016-08-15
-- Author: A. Patel (Database Architecture Team)
-- Database: MS SQL Server 2014 / 2016
-- ============================================================================
-- $Header: /cvsroot/trading/db/schema_v4_current.sql,v 1.56 2019/04/10 11:45:00 apatel Exp $
-- $Revision: 1.56 $
-- ============================================================================
--
-- MIGRATION NOTES:
-- This migration adds full trade lifecycle tracking, audit trail,
-- and regulatory reporting enhancements for EMIR/MIFID II compliance.
--
-- IMPORTANT: This schema requires SQL Server 2014 or later
-- Uses features: MEMORY_OPTIMIZED tables, COLUMNSTORE indexes
--
-- PREREQUISITES:
-- 1. Run schema_v1_initial.sql
-- 2. Run schema_v2_counterparty.sql  
-- 3. Run schema_v3_notional.sql
-- 4. Ensure database compatibility level >= 120
--
-- ============================================================================

-- Version tracking table
IF NOT EXISTS (SELECT * FROM sysobjects WHERE name = 'SCHEMA_VERS' AND type = 'U')
CREATE TABLE SCHEMA_VERS (
    SCHEMA_VERS_ID   INT IDENTITY(1,1) NOT NULL,
    VERS_NUM         VARCHAR(16) NOT NULL,
    APPLIED_DT       DATETIME DEFAULT GETDATE(),
    APPLIED_BY       VARCHAR(32) NOT NULL,
    NOTES            VARCHAR(256) NULL,
    
    CONSTRAINT PK_SCHEMA_VERS PRIMARY KEY (SCHEMA_VERS_ID)
)
GO

INSERT INTO SCHEMA_VERS (VERS_NUM, APPLIED_BY, NOTES) 
VALUES ('4.0.0', 'SYSTEM', 'Initial V4 schema')
GO

BEGIN TRANSACTION
GO

PRINT 'Starting Schema V4 Migration - Current Version'
PRINT GETDATE()
GO

-- ============================================================================
-- Add trade lifecycle tracking columns
-- ============================================================================
IF NOT EXISTS (SELECT * FROM syscolumns 
               WHERE id = OBJECT_ID('TRD_BLOTTER') AND name = 'TRD_VERS_NUM')
BEGIN
    ALTER TABLE TRD_BLOTTER ADD TRD_VERS_NUM INT DEFAULT 1
    PRINT 'Added TRD_VERS_NUM column'
END
GO

IF NOT EXISTS (SELECT * FROM syscolumns 
               WHERE id = OBJECT_ID('TRD_BLOTTER') AND name = 'ORIG_TRD_ID')
BEGIN
    -- For amendments, points to original trade
    ALTER TABLE TRD_BLOTTER ADD ORIG_TRD_ID INT NULL
    PRINT 'Added ORIG_TRD_ID column'
END
GO

IF NOT EXISTS (SELECT * FROM syscolumns 
               WHERE id = OBJECT_ID('TRD_BLOTTER') AND name = 'AMND_SEQ_NUM')
BEGIN
    ALTER TABLE TRD_BLOTTER ADD AMND_SEQ_NUM INT DEFAULT 0
    PRINT 'Added AMND_SEQ_NUM column'
END
GO

IF NOT EXISTS (SELECT * FROM syscolumns 
               WHERE id = OBJECT_ID('TRD_BLOTTER') AND name = 'USI_CD')
BEGIN
    -- Unique Swap Identifier for Dodd-Frank
    ALTER TABLE TRD_BLOTTER ADD USI_CD VARCHAR(52) NULL
    PRINT 'Added USI_CD column'
END
GO

IF NOT EXISTS (SELECT * FROM syscolumns 
               WHERE id = OBJECT_ID('TRD_BLOTTER') AND name = 'UTI_CD')
BEGIN
    -- Unique Transaction Identifier for EMIR
    ALTER TABLE TRD_BLOTTER ADD UTI_CD VARCHAR(52) NULL
    PRINT 'Added UTI_CD column'
END
GO

IF NOT EXISTS (SELECT * FROM syscolumns 
               WHERE id = OBJECT_ID('TRD_BLOTTER') AND name = 'CLR_STS_CD')
BEGIN
    -- Clearing status: UNC, CLR, RJC
    ALTER TABLE TRD_BLOTTER ADD CLR_STS_CD CHAR(3) DEFAULT 'UNC'
    PRINT 'Added CLR_STS_CD column'
END
GO

IF NOT EXISTS (SELECT * FROM syscolumns 
               WHERE id = OBJECT_ID('TRD_BLOTTER') AND name = 'CLR_HOUSE_CD')
BEGIN
    ALTER TABLE TRD_BLOTTER ADD CLR_HOUSE_CD VARCHAR(16) NULL
    PRINT 'Added CLR_HOUSE_CD column'
END
GO

IF NOT EXISTS (SELECT * FROM syscolumns 
               WHERE id = OBJECT_ID('TRD_BLOTTER') AND name = 'PORTFOLIO_ID')
BEGIN
    ALTER TABLE TRD_BLOTTER ADD PORTFOLIO_ID INT NULL
    PRINT 'Added PORTFOLIO_ID column'
END
GO

-- ============================================================================
-- Trade history table (audit trail)
-- ============================================================================
IF EXISTS (SELECT * FROM sysobjects WHERE name = 'TRD_HIST' AND type = 'U')
    DROP TABLE TRD_HIST
GO

CREATE TABLE TRD_HIST (
    TRD_HIST_ID      BIGINT IDENTITY(1,1) NOT NULL,
    TRD_ID           INT NOT NULL,
    TRD_VERS_NUM     INT NOT NULL,
    TRD_STTUS_CD     CHAR(2) NOT NULL,
    NTNL_AMT         DECIMAL(22,2) NULL,
    MTM_VAL          DECIMAL(18,6) NULL,
    CHNG_TYPE_CD     VARCHAR(8) NOT NULL,  -- NEW, AMND, CNCL, etc.
    CHNG_REASON_TX   VARCHAR(256) NULL,
    CHNG_BY_USR      VARCHAR(32) NOT NULL,
    CHNG_TS          DATETIME DEFAULT GETDATE(),
    
    CONSTRAINT PK_TRD_HIST PRIMARY KEY (TRD_HIST_ID)
)
GO

-- Partition by change date for performance
-- CREATE PARTITION FUNCTION PF_TRD_HIST_TS (DATETIME)
-- AS RANGE RIGHT FOR VALUES ('20160101', '20170101', '20180101', '20190101')
-- GO

CREATE INDEX IDX_TRD_HIST_TRD ON TRD_HIST(TRD_ID)
GO

CREATE INDEX IDX_TRD_HIST_TS ON TRD_HIST(CHNG_TS)
GO

-- ============================================================================
-- Trade allocation table
-- ============================================================================
IF EXISTS (SELECT * FROM sysobjects WHERE name = 'TRD_ALLOC' AND type = 'U')
    DROP TABLE TRD_ALLOC
GO

CREATE TABLE TRD_ALLOC (
    TRD_ALLOC_ID     INT IDENTITY(1,1) NOT NULL,
    TRD_ID           INT NOT NULL,
    ALLOC_PCT        DECIMAL(8,6) NOT NULL,
    ALLOC_NTNL_AMT   DECIMAL(22,2) NOT NULL,
    BOOK_NM          VARCHAR(16) NOT NULL,
    FUND_CD          VARCHAR(16) NULL,
    STRTGY_CD        VARCHAR(16) NULL,
    CRTD_TS          DATETIME DEFAULT GETDATE(),
    UPDTD_TS         DATETIME NULL,
    
    CONSTRAINT PK_TRD_ALLOC PRIMARY KEY (TRD_ALLOC_ID),
    CONSTRAINT FK_TRD_ALLOC_TRD FOREIGN KEY (TRD_ID) 
        REFERENCES TRD_BLOTTER(TRD_ID)
)
GO

CREATE INDEX IDX_TRD_ALLOC_TRD ON TRD_ALLOC(TRD_ID)
GO

-- ============================================================================
-- Portfolio reference table
-- ============================================================================
IF EXISTS (SELECT * FROM sysobjects WHERE name = 'PORTFOLIO_REF' AND type = 'U')
    DROP TABLE PORTFOLIO_REF
GO

CREATE TABLE PORTFOLIO_REF (
    PORTFOLIO_ID     INT IDENTITY(1,1) NOT NULL,
    PORTFOLIO_CD     VARCHAR(16) NOT NULL,
    PORTFOLIO_NM     VARCHAR(64) NOT NULL,
    DESK_NM          VARCHAR(32) NOT NULL,
    PORTFOLIO_TYP_CD VARCHAR(8) NOT NULL,
    BASE_CCY_CD      CHAR(3) DEFAULT 'USD',
    ACTV_FLG         CHAR(1) DEFAULT 'Y',
    CRTD_TS          DATETIME DEFAULT GETDATE(),
    UPDTD_TS         DATETIME NULL,
    
    CONSTRAINT PK_PORTFOLIO_REF PRIMARY KEY (PORTFOLIO_ID),
    CONSTRAINT UK_PORTFOLIO_REF_CD UNIQUE (PORTFOLIO_CD)
)
GO

-- ============================================================================
-- Add clearing house reference
-- ============================================================================
IF EXISTS (SELECT * FROM sysobjects WHERE name = 'CLR_HOUSE_REF' AND type = 'U')
    DROP TABLE CLR_HOUSE_REF
GO

CREATE TABLE CLR_HOUSE_REF (
    CLR_HOUSE_CD     VARCHAR(16) NOT NULL,
    CLR_HOUSE_NM     VARCHAR(64) NOT NULL,
    CLR_HOUSE_LEI    VARCHAR(20) NULL,
    ACTV_FLG         CHAR(1) DEFAULT 'Y',
    
    CONSTRAINT PK_CLR_HOUSE_REF PRIMARY KEY (CLR_HOUSE_CD)
)
GO

INSERT INTO CLR_HOUSE_REF (CLR_HOUSE_CD, CLR_HOUSE_NM) VALUES 
    ('LCH', 'LCH.Clearnet')
INSERT INTO CLR_HOUSE_REF (CLR_HOUSE_CD, CLR_HOUSE_NM) VALUES 
    ('CME', 'CME Clearing')
INSERT INTO CLR_HOUSE_REF (CLR_HOUSE_CD, CLR_HOUSE_NM) VALUES 
    ('ICE', 'ICE Clear Credit')
INSERT INTO CLR_HOUSE_REF (CLR_HOUSE_CD, CLR_HOUSE_NM) VALUES 
    ('EUREX', 'Eurex Clearing')
GO

-- ============================================================================
-- Add change type reference
-- ============================================================================
IF EXISTS (SELECT * FROM sysobjects WHERE name = 'CHNG_TYP_REF' AND type = 'U')
    DROP TABLE CHNG_TYP_REF
GO

CREATE TABLE CHNG_TYP_REF (
    CHNG_TYP_CD      VARCHAR(8) NOT NULL,
    CHNG_TYP_NM      VARCHAR(32) NOT NULL,
    CHNG_TYP_DESC    VARCHAR(128) NULL,
    
    CONSTRAINT PK_CHNG_TYP_REF PRIMARY KEY (CHNG_TYP_CD)
)
GO

INSERT INTO CHNG_TYP_REF (CHNG_TYP_CD, CHNG_TYP_NM) VALUES ('NEW', 'New Trade')
INSERT INTO CHNG_TYP_REF (CHNG_TYP_CD, CHNG_TYP_NM) VALUES ('AMND', 'Amendment')
INSERT INTO CHNG_TYP_REF (CHNG_TYP_CD, CHNG_TYP_NM) VALUES ('CNCL', 'Cancellation')
INSERT INTO CHNG_TYP_REF (CHNG_TYP_CD, CHNG_TYP_NM) VALUES ('NOV', 'Novation')
INSERT INTO CHNG_TYP_REF (CHNG_TYP_CD, CHNG_TYP_NM) VALUES ('CLR', 'Cleared')
INSERT INTO CHNG_TYP_REF (CHNG_TYP_CD, CHNG_TYP_NM) VALUES ('VAL', 'Valuation Update')
INSERT INTO CHNG_TYP_REF (CHNG_TYP_CD, CHNG_TYP_NM) VALUES ('STTS', 'Status Change')
GO

-- ============================================================================
-- Add clearing status reference
-- ============================================================================
IF EXISTS (SELECT * FROM sysobjects WHERE name = 'CLR_STS_REF' AND type = 'U')
    DROP TABLE CLR_STS_REF
GO

CREATE TABLE CLR_STS_REF (
    CLR_STS_CD       CHAR(3) NOT NULL,
    CLR_STS_NM       VARCHAR(32) NOT NULL,
    
    CONSTRAINT PK_CLR_STS_REF PRIMARY KEY (CLR_STS_CD)
)
GO

INSERT INTO CLR_STS_REF (CLR_STS_CD, CLR_STS_NM) VALUES ('UNC', 'Uncleared')
INSERT INTO CLR_STS_REF (CLR_STS_CD, CLR_STS_NM) VALUES ('CLR', 'Cleared')
INSERT INTO CLR_STS_REF (CLR_STS_CD, CLR_STS_NM) VALUES ('RJC', 'Rejected')
INSERT INTO CLR_STS_REF (CLR_STS_CD, CLR_STS_NM) VALUES ('PND', 'Pending')
GO

-- ============================================================================
-- Add foreign key from TRD_BLOTTER to PORTFOLIO_REF
-- ============================================================================
-- NOTE: Adding FK to existing table with data requires validation
-- ============================================================================
-- ALTER TABLE TRD_BLOTTER ADD CONSTRAINT FK_TRD_BLOTTER_PORTFOLIO
--     FOREIGN KEY (PORTFOLIO_ID) REFERENCES PORTFOLIO_REF(PORTFOLIO_ID)
-- GO

-- ============================================================================
-- Add indexes for new columns
-- ============================================================================
CREATE INDEX IDX_TRD_BLOTTER_USI ON TRD_BLOTTER(USI_CD)
GO

CREATE INDEX IDX_TRD_BLOTTER_UTI ON TRD_BLOTTER(UTI_CD)
GO

CREATE INDEX IDX_TRD_BLOTTER_CLR_STS ON TRD_BLOTTER(CLR_STS_CD)
GO

CREATE INDEX IDX_TRD_BLOTTER_ORIG ON TRD_BLOTTER(ORIG_TRD_ID)
GO

-- ============================================================================
-- Update version tracking
-- ============================================================================
INSERT INTO SCHEMA_VERS (VERS_NUM, APPLIED_BY, NOTES) 
VALUES ('4.0.0', 'SYSTEM', 'V4 migration completed')
GO

PRINT 'Schema V4 Migration Complete'
PRINT GETDATE()
GO

COMMIT TRANSACTION
GO

-- ============================================================================
-- VIEWS FOR BACKWARDS COMPATIBILITY
-- ============================================================================

-- View for legacy applications that expect old column names
IF EXISTS (SELECT * FROM sysobjects WHERE name = 'V_TRD_BLOTTER_LEGACY' AND type = 'V')
    DROP VIEW V_TRD_BLOTTER_LEGACY
GO

CREATE VIEW V_TRD_BLOTTER_LEGACY AS
SELECT 
    TRD_ID AS trade_id,
    TRD_REF_NM AS trade_ref,
    CPTY_ID AS counterparty_id,
    PRD_TYP_ID AS product_type_id,
    TRD_STTUS_CD AS status_code,
    TRD_DT AS trade_date,
    CCY_CD AS currency,
    NTNL_AMT AS notional_amount,
    CRTD_TS AS created_timestamp
FROM TRD_BLOTTER
GO

-- ============================================================================
-- STORED PROCEDURES (see sp_trade_validation.sql for details)
-- ============================================================================

-- $Log: schema_v4_current.sql,v $
-- Revision 1.56  2019/04/10 11:45:00  apatel
-- Added portfolio and allocation support
--
-- Revision 1.55  2018/11/22 14:30:00  apatel
-- Added clearing house reference
--
-- Revision 1.54  2018/06/15 09:00:00  apatel
-- Added USI/UTI for regulatory reporting
--
-- Revision 1.53  2016/08/15 10:00:00  apatel
-- Initial V4 schema with lifecycle tracking
-- ============================================================================
